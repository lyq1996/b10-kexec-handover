
#include "common.h"
#include "fdt.h"
#include "lzma.h"
#include "container.h"
#include "kexec-lite.h"

#define IMG_CAP      (32UL << 20)
#define FIT_CAP      (16UL << 20)
#define DTB_MAX      (1UL << 20)
#define KO_MAX       (1UL << 20)
#define MOD_DTB_MAX  (128UL << 10)

static u8 img_buf[IMG_CAP] __attribute__((aligned(4096)));
static u8 fit_buf[FIT_CAP] __attribute__((aligned(4096)));
static u8 dtb_buf[DTB_MAX] __attribute__((aligned(4096)));
static u8 ko_buf[KO_MAX] __attribute__((aligned(4096)));
static char clibuf[4096];

struct cli_opts {
    const char *ko, *kernel, *dtb, *cmdline, *append, *bootlog;
    const char *dump_image, *dump_dtb;
    u64 mem_base, mem_size;
    int info, load_only;
};

__asm__(
    ".global _start\n"
    "_start:\n"
    "   mov x0, sp\n"
    "   b real_start\n"
);

static int check_abi(long devfd)
{
    struct kl_abi_info vi;
    s64 r = sys_ioctl(devfd, KL_IOC_VERSION, &vi);

    if (r == -ENOTTY) {
        out("kjump: module provides no ABI version query (old build), continuing as ABI v1\n");
        return 0;
    }
    if (r < 0) {
        out("kjump: ABI version query failed errno=");
        out_hex("", (u64)(-r));
        out("\n");
        return -1;
    }
    if (vi.version != KL_ABI_VERSION || vi.load_size != sizeof(struct kl_load)) {
        out("kjump: ABI mismatch — module reports v");
        out_hex("", vi.version);
        out(" / ");
        out_hex("", vi.load_size);
        out(" bytes, expected v");
        out_hex("", (u64)KL_ABI_VERSION);
        out(" / ");
        out_hex("", (u64)sizeof(struct kl_load));
        out(" bytes; rebuild the module (rc=11)\n");
        return -1;
    }
    return 0;
}

static void usage(void)
{
    out("usage: kjump --kernel <src> [--ko <kexec-lite.ko>] [options]\n"
        "  --dtb <path>             device tree (default /sys/firmware/fdt)\n"
        "  --cmdline \"<str>\"        replace /chosen/bootargs\n"
        "  --append-cmdline \"<str>\" append to existing bootargs\n"
        "  --mem-base <hex>         load base (default 0x40000000)\n"
        "  --mem-size <hex>         reserved window (default 0x40000000)\n"
        "  --bootlog <path>         forensic log, per-line fsync\n"
        "  --info                   parse and print only\n"
        "  --load-only              load via module, keep system running\n"
        "  --dump-image <path>      write extracted kernel image\n"
        "  --dump-dtb <path>        write final (injected) dtb\n"
        "kernel source: bare arm64 Image | FIT | ELF32 container (auto)\n"
        "exit: 1 usage 2 ko 3 insmod 4 dtb 5 nodev 6 LOAD 7 JUMP-returned\n"
        "      8 cpus 9 kernel-src 10 cmdline 11 abi\n");
}

static int streq0(const char *a, const char *b)
{
    u64 i;
    for (i = 0; a[i] && a[i] == b[i]; i++) ;
    return a[i] == b[i];
}

static s64 parse_u64(const char *s, u64 *out_v)
{
    u64 v = 0;
    if (!s || !*s) return -1;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    for (; *s; s++) {
        char c = *s;
        u64 d;
        if (c >= '0' && c <= '9') d = (u64)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (u64)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (u64)(c - 'A' + 10);
        else return -1;
        v = (v << 4) | d;
    }
    *out_v = v;
    return 0;
}

static int opt_val(int argc, char **argv, int *i, const char *name,
                   const char **out_val)
{
    const char *a = argv[*i];
    u64 n = slen(name);
    if (slen(a) < n || memcmp(a, name, n) != 0) return 0;
    if (a[n] == '=') { *out_val = a + n + 1; return 1; }
    if (a[n] != 0) return 0;
    if (*i + 1 >= argc) {
        out("kjump: option needs a value: ");
        out(name);
        out("\n");
        return -1;
    }
    *out_val = argv[++(*i)];
    return 1;
}

static int parse_cli(int argc, char **argv, struct cli_opts *o)
{
    int i, r;

    o->ko = o->kernel = o->cmdline = o->append = o->bootlog = 0;
    o->dump_image = o->dump_dtb = 0;
    o->info = o->load_only = 0;
    o->dtb = "/sys/firmware/fdt";
    o->mem_base = 0x40000000UL;
    o->mem_size = 0x40000000UL;

    for (i = 1; i < argc; i++) {
        const char *v;
        if ((r = opt_val(argc, argv, &i, "--kernel", &v))) {
            if (r < 0) return -1;
            o->kernel = v;
        } else if ((r = opt_val(argc, argv, &i, "--ko", &v))) {
            if (r < 0) return -1;
            o->ko = v;
        } else if ((r = opt_val(argc, argv, &i, "--dtb", &v))) {
            if (r < 0) return -1;
            o->dtb = v;
        } else if ((r = opt_val(argc, argv, &i, "--cmdline", &v))) {
            if (r < 0) return -1;
            o->cmdline = v;
        } else if ((r = opt_val(argc, argv, &i, "--append-cmdline", &v))) {
            if (r < 0) return -1;
            o->append = v;
        } else if ((r = opt_val(argc, argv, &i, "--mem-base", &v))) {
            if (r < 0) return -1;
            if (parse_u64(v, &o->mem_base) < 0) {
                out("kjump: bad --mem-base\n");
                return -1;
            }
        } else if ((r = opt_val(argc, argv, &i, "--mem-size", &v))) {
            if (r < 0) return -1;
            if (parse_u64(v, &o->mem_size) < 0) {
                out("kjump: bad --mem-size\n");
                return -1;
            }
        } else if ((r = opt_val(argc, argv, &i, "--bootlog", &v))) {
            if (r < 0) return -1;
            o->bootlog = v;
        } else if ((r = opt_val(argc, argv, &i, "--dump-image", &v))) {
            if (r < 0) return -1;
            o->dump_image = v;
        } else if ((r = opt_val(argc, argv, &i, "--dump-dtb", &v))) {
            if (r < 0) return -1;
            o->dump_dtb = v;
        } else if (streq0(argv[i], "--info")) {
            o->info = 1;
        } else if (streq0(argv[i], "--load-only")) {
            o->load_only = 1;
        } else {
            out("kjump: unknown option: ");
            out(argv[i]);
            out("\n");
            return -1;
        }
    }
    if (!o->kernel) {
        out("kjump: --kernel is required\n");
        return -1;
    }
    if (!o->ko && !o->info) {
        out("kjump: --ko is required (or use --info)\n");
        return -1;
    }
    if (o->cmdline && o->append) {
        out("kjump: --cmdline and --append-cmdline are mutually exclusive\n");
        return -1;
    }
    return 0;
}

static int write_full(long fd, const u8 *buf, u64 len)
{
    u64 done = 0;
    while (done < len) {
        s64 n = sys_write(fd, buf + done, len - done);
        if (n <= 0) return -1;
        done += (u64)n;
    }
    return 0;
}

static int writefile(const char *path, const u8 *buf, u64 len)
{
    s64 fd = sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (write_full(fd, buf, len) < 0) { sys_close(fd); return -1; }
    sys_close(fd);
    return 0;
}

static void out_first16(const u8 *p)
{
    static const char hexd[] = "0123456789abcdef";
    char b[33];
    int i;
    for (i = 0; i < 16; i++) {
        b[i * 2]     = hexd[p[i] >> 4];
        b[i * 2 + 1] = hexd[p[i] & 0xF];
    }
    b[32] = 0;
    out(b);
}

static void out_strn(const char *s, u64 max)
{
    u64 n = 0;
    while (n < max && s[n]) n++;
    sys_write(2, s, n);
}

static const char *build_cmdline(struct cli_opts *o, struct fdt_blob *db,
                                 char *buf, u64 bufsz)
{
    const u8 *old;
    u32 old_len;
    u64 n = 0;

    if (o->cmdline)
        return o->cmdline;

    if (fdt_chosen_bootargs(db, &old, &old_len) == 0) {
        u32 t = 0;
        while (t < old_len && old[t]) {
            if (n + 1 >= bufsz) goto too_long;
            buf[n++] = old[t++];
        }
        if (n && n + 2 < bufsz)
            buf[n++] = ' ';
    }
    {
        const char *s = o->append;
        u64 i;
        for (i = 0; s[i]; i++) {
            if (n + 1 >= bufsz) goto too_long;
            buf[n++] = s[i];
        }
    }
    buf[n] = 0;
    return buf;

too_long:
    out("kjump: joined cmdline exceeds 4KB buffer (rc=10)\n");
    return 0;
}

static int info_mode(struct cli_opts *o)
{
    struct ksrc ks;
    struct fdt_blob db;
    char note[128];
    u64 dtb_len = 0, src_size;
    long fd;
    int r;

    fd = sys_open(o->kernel, O_RDONLY, 0);
    if (fd < 0) {
        out("kjump: cannot open kernel source: ");
        out(o->kernel);
        out("\n");
        return 9;
    }
    src_size = file_size(fd);
    note[0] = 0;
    if (src_size == 0 ||
        container_load_kernel(fd, src_size, img_buf, IMG_CAP,
                              fit_buf, FIT_CAP, &ks, note, sizeof(note)) < 0) {
        out("kjump: kernel source parse failed: ");
        out(note);
        out("\n");
        sys_close(fd);
        return 9;
    }
    sys_close(fd);

    out("kernel source : ");
    out(o->kernel);
    out("\n");
    out_hex("  image len   = ", ks.image_len);
    out_hex("  text_offset = ", ks.text_off);
    out_hex("  image_size  = ", ks.image_size);
    out("  first16     : ");
    out_first16(ks.image);
    out("\n");

    r = readfile(o->dtb, dtb_buf, DTB_MAX, &dtb_len);
    if (r < 0) {
        out("kjump: cannot read dtb: ");
        out(o->dtb);
        out("\n");
        return 4;
    }
    if (r == 1) {
        out("kjump: dtb exceeds 1MB buffer\n");
        return 4;
    }
    if (fdt_open(&db, dtb_buf, DTB_MAX) < 0) {
        out("kjump: dtb is not a valid FDT\n");
        return 4;
    }
    {
        const u8 *ba;
        u32 ba_len;
        r = fdt_chosen_bootargs(&db, &ba, &ba_len);
        out("current bootargs: ");
        if (r == 0 && ba_len)
            out_strn((const char *)ba, ba_len);
        else
            out("(none)");
        out("\n");
    }

    if (o->cmdline || o->append) {
        const char *final = build_cmdline(o, &db, clibuf, sizeof(clibuf));
        if (!final)
            return 10;
        note[0] = 0;
        if (fdt_chosen_set_bootargs(&db, final, note, sizeof(note)) < 0) {
            out("kjump: bootargs inject failed: ");
            out(note);
            out("\n");
            return 10;
        }
        out("final bootargs  : ");
        out(final);
        out("\n");
    }
    dtb_len = fdt_finalize(&db);
    out_hex("  dtb len     = ", dtb_len);
    if (dtb_len > MOD_DTB_MAX) {
        out("kjump: dtb exceeds module 128KB slot (rc=10)\n");
        return 10;
    }

    if (o->dump_image) {
        if (writefile(o->dump_image, ks.image, ks.image_len) < 0) {
            out("kjump: --dump-image write failed\n");
            return 9;
        }
        out("dumped image -> ");
        out(o->dump_image);
        out("\n");
    }
    if (o->dump_dtb) {
        if (writefile(o->dump_dtb, dtb_buf, dtb_len) < 0) {
            out("kjump: --dump-dtb write failed\n");
            return 4;
        }
        out("dumped dtb   -> ");
        out(o->dump_dtb);
        out("\n");
    }
    return 0;
}

#define CPU_PFX "/sys/devices/system/cpu/cpu"
static int offline_secondaries(void)
{
    static const char pfx[] = CPU_PFX;
    static const char sfx[] = "/online";
    char path[sizeof(CPU_PFX) + 2 + sizeof(sfx)];
    int n, still = 0;
    for (n = 1; n < 64; n++) {
        s64 fd, r;
        char buf[4];
        int i, j, dirlen;
        for (i = 0; pfx[i]; i++) path[i] = pfx[i];
        if (n >= 10)
            path[i++] = (char)('0' + n / 10);
        path[i++] = (char)('0' + n % 10);
        dirlen = i;
        for (j = 0; sfx[j]; j++) path[i++] = sfx[j];
        path[i] = '\0';
        fd = sys_open(path, O_WRONLY, 0);
        if (fd >= 0) {
            sys_write(fd, "0\n", 2);
            sys_close(fd);
            fd = sys_open(path, O_RDONLY, 0);
            if (fd < 0) { still++; continue; }
            r = sys_read(fd, buf, 3);
            sys_close(fd);
            if (r <= 0 || buf[0] == '1')
                still++;
        } else {
            path[dirlen] = '\0';
            fd = sys_open(path, O_RDONLY, 0);
            path[dirlen] = '/';
            if (fd >= 0) {
                sys_close(fd);
                still++;
            }
        }
    }
    return still;
}

static u64 quiesce_pci(void)
{
    static const char dir[] = "/sys/bus/pci/devices";
    static const char cfg[] = "/config";
    char path[96], devid[32];
    u8 buf[1024];
    u8 zero[2] = {0, 0};
    u64 done = 0;
    s64 dfd = sys_open(dir, O_RDONLY, 0);
    s64 n, pos;
    if (dfd < 0) return 0;
    for (;;) {
        n = sys_getdents64(dfd, buf, sizeof(buf));
        if (n <= 0) break;
        for (pos = 0; pos + 19 < n; ) {

            u16 reclen = (u16)(buf[pos + 16] | ((u16)buf[pos + 17] << 8));
            char *name = (char *)(buf + pos + 19);
            u64 o = 0, l = 0;
            s64 fd;
            if (reclen < 20) break;
            while (name[l] && l < 30) { devid[l] = name[l]; l++; }
            devid[l] = '\0';
            if (l >= 3 && devid[0] != '.') {
                o = scpy(path, dir, sizeof(path) - 1);
                path[o++] = '/';
                o += scpy(path + o, devid, sizeof(path) - o - 1);
                o += scpy(path + o, cfg, sizeof(path) - o - 1);
                path[o] = '\0';
                fd = sys_open(path, O_WRONLY, 0);
                if (fd >= 0) {
                    sys_pwrite64(fd, zero, 2, 4);
                    sys_close(fd);
                    done++;
                }
            }
            pos += reclen;
        }
    }
    sys_close(dfd);
    return done;
}

__attribute__((used)) void real_start(u64 *sp)
{
    u64 argc = sp[0];
    char **argv = (char **)(sp + 1);
    struct cli_opts o;
    struct ksrc ks;
    struct fdt_blob db;
    struct kl_load k;
    char note[128];
    u64 dtb_len = 0, ko_len = 0, src_size;
    s64 fd, devfd, r;
    int still;

    if (parse_cli((int)argc, argv, &o) < 0) {
        usage();
        sys_exit(1);
    }

    if (o.bootlog)
        klog_open(o.bootlog);
    {
        static char linebuf[1024];
        u64 n = 0;
        int i;
        for (i = 0; i < (int)argc && n + 4 < sizeof(linebuf); i++) {
            const char *s = argv[i];
            u64 j;
            if (i) linebuf[n++] = ' ';
            linebuf[n++] = '"';
            for (j = 0; s[j] && n + 3 < sizeof(linebuf); j++)
                linebuf[n++] = s[j];
            linebuf[n++] = '"';
        }
        linebuf[n] = 0;
        klog(linebuf);
    }

    if (o.info) {
        int rc = info_mode(&o);
        sys_exit(rc);
    }

    do_sync();

    out("kjump[1/6] parsing kernel source...\n");
    fd = sys_open(o.kernel, O_RDONLY, 0);
    if (fd < 0) {
        out("kjump: cannot open kernel source: ");
        out(o.kernel);
        out("\n");
        sys_exit(9);
    }
    src_size = file_size(fd);
    note[0] = 0;
    if (src_size == 0 ||
        container_load_kernel(fd, src_size, img_buf, IMG_CAP,
                              fit_buf, FIT_CAP, &ks, note, sizeof(note)) < 0) {
        out("kjump: kernel source parse failed: ");
        out(note);
        out("\n");
        sys_exit(9);
    }
    sys_close(fd);
    klog("kjump: kernel source ok");
    klog_hex("kjump: image_len=", ks.image_len);

    out("kjump[2/6] reading dtb + injecting cmdline...\n");
    r = readfile(o.dtb, dtb_buf, DTB_MAX, &dtb_len);
    if (r < 0) {
        out("kjump: cannot read dtb: ");
        out(o.dtb);
        out("\n");
        sys_exit(4);
    }
    if (r == 1) {
        out("kjump: dtb exceeds 1MB buffer\n");
        sys_exit(4);
    }
    if (fdt_open(&db, dtb_buf, DTB_MAX) < 0) {
        out("kjump: dtb is not a valid FDT\n");
        sys_exit(4);
    }
    if (o.cmdline || o.append) {
        const char *final = build_cmdline(&o, &db, clibuf, sizeof(clibuf));
        if (!final)
            sys_exit(10);
        note[0] = 0;
        if (fdt_chosen_set_bootargs(&db, final, note, sizeof(note)) < 0) {
            out("kjump: bootargs inject failed: ");
            out(note);
            out("\n");
            sys_exit(10);
        }
        klog_raw("kjump: bootargs =");
        klog(final);
    }
    dtb_len = fdt_finalize(&db);
    if (dtb_len > MOD_DTB_MAX) {
        out("kjump: dtb exceeds module 128KB slot (rc=10)\n");
        sys_exit(10);
    }
    klog_hex("kjump: dtb_len=", dtb_len);

    if (o.dump_image) {
        if (writefile(o.dump_image, ks.image, ks.image_len) < 0) {
            out("kjump: --dump-image write failed\n");
            sys_exit(9);
        }
        klog("kjump: image dumped");
    }
    if (o.dump_dtb) {
        if (writefile(o.dump_dtb, dtb_buf, dtb_len) < 0) {
            out("kjump: --dump-dtb write failed\n");
            sys_exit(4);
        }
        klog("kjump: dtb dumped");
    }

    if (o.load_only) {
        out("kjump[3/6] load-only: skipping cpu offline\n");
    } else {
        out("kjump[3/6] offlining secondary cpus...\n");
        still = offline_secondaries();
        if (still) {
            out("kjump: ");
            out_hex("", (u64)still);
            out(" secondary cpu(s) still online — refusing to jump (rc=8)\n");
            sys_exit(8);
        }
        klog("kjump: cpus offlined");
    }

    out("kjump[4/6] loading module + LOAD...\n");
    r = readfile(o.ko, ko_buf, KO_MAX, &ko_len);
    if (r < 0) {
        out("kjump: cannot read ko: ");
        out(o.ko);
        out("\n");
        sys_exit(2);
    }
    if (r == 1) {
        out("kjump: ko exceeds 1MB buffer\n");
        sys_exit(2);
    }
    r = sys_init_module(ko_buf, ko_len, "");
    if (r == -EEXIST) {

        out("kjump: module already loaded, reusing\n");
    } else if (r < 0) {
        out("kjump: init_module failed errno=");
        out_hex("", (u64)(-r));
        out("  (vermagic/struct mismatch? must be built from the same source tree as the target kernel) (rc=3)\n");
        sys_exit(3);
    }

    devfd = sys_open("/dev/kexec-lite", O_RDWR, 0);
    for (int tries = 0; devfd < 0 && tries < 40; tries++) {
        msleep(50);
        devfd = sys_open("/dev/kexec-lite", O_RDWR, 0);
    }
    if (devfd < 0) {
        out("kjump: no /dev/kexec-lite after 2s (hotplug dead? try: mknod /dev/kexec-lite c 10 $(grep kexec-lite /proc/misc | cut -d' ' -f1)) (rc=5)\n");
        sys_exit(5);
    }
    if (check_abi(devfd) < 0)
        sys_exit(11);
    k.img_ptr  = (u64)ks.image;
    k.img_len  = ks.image_len;
    k.dtb_ptr  = (u64)dtb_buf;
    k.dtb_len  = dtb_len;
    k.mem_base = o.mem_base;
    k.mem_size = o.mem_size;
    r = sys_ioctl(devfd, KL_IOC_LOAD, &k);
    if (r < 0) {
        out("kjump: LOAD failed errno=");
        out_hex("", (u64)(-r));
        out("  (not enough contiguous physical memory? increase mem_size or boot earlier) (rc=6)\n");
        sys_exit(6);
    }
    klog("kjump: LOAD ok (kernel+dtb copied into reserved memory)");
    if (o.load_only) {
        out("kjump: load-only complete — system stays usable; rmmod kexec_lite to release\n");
        sys_exit(0);
    }

    klog("kjump: quiescing pci then JUMP — disk goes dark here");
    out("kjump[5/6] quiescing pci devices...\n");
    {
        u64 nq = quiesce_pci();
        out("kjump: pci COMMAND=0 written on ");
        out_hex("", nq);
        out(" device(s)\n");
    }

    out("kjump[6/6] JUMP...\n");
    r = sys_ioctl(devfd, KL_IOC_JUMP, 0);
    out("kjump: JUMP returned errno=");
    out_hex("", (u64)(-r));
    out("  — handover failed, staying on stock firmware (rc=7)\n");
    sys_exit(7);
}

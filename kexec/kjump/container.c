
#include "container.h"
#include "fdt.h"
#include "lzma.h"

#define PROBE_CAP (64u * 1024u)
static u8 g_probe[PROBE_CAP];

#define ARM64_MAGIC_OFF  0x38
#define ARM64_MAGIC_LE   0x644d5241u

static int has_arm64_magic(const u8 *p, u64 len)
{
    return len >= ARM64_MAGIC_OFF + 4 &&
           le32ld(p + ARM64_MAGIC_OFF) == ARM64_MAGIC_LE;
}

static int parse_arm64(const u8 *img, u64 len, struct ksrc *ks,
                       char *note, int note_sz)
{
    if (len < 64) {
        scpy(note, "Image shorter than 64 bytes", note_sz);
        return -1;
    }
    if (le32ld(img + ARM64_MAGIC_OFF) != ARM64_MAGIC_LE) {
        scpy(note, "missing arm64 Image magic (ARM\\x64 @0x38)", note_sz);
        return -1;
    }
    ks->text_off   = le64ld(img + 0x08);
    ks->image_size = le64ld(img + 0x10);
    if ((ks->text_off & 0xFFFULL) || ks->text_off > (2ULL << 20)) {
        scpy(note, "abnormal text_offset (not 4K-aligned or over 2MB)", note_sz);
        return -1;
    }
    ks->image = img;
    ks->image_len = len;

    if (ks->image_size && ks->image_size < len)
        ks->image_len = ks->image_size;
    return 0;
}

static int extract_from_fit(u8 *fit, u64 fit_len,
                            u8 *img, u64 img_cap,
                            struct ksrc *ks, char *note, int note_sz)
{
    struct fdt_blob b;
    struct fit_kernel fk;
    int r;

    if (fdt_open(&b, fit, fit_len) < 0) {
        scpy(note, "FIT is not a valid FDT", note_sz);
        return -1;
    }
    r = fdt_fit_find_kernel(&b, &fk);
    if (r == -2) { scpy(note, "corrupt FIT structure", note_sz); return -1; }
    if (r < 0)   { scpy(note, "no type=kernel node in FIT", note_sz); return -1; }
    if (fk.comp < 0) {
        scpy(note, "unsupported FIT kernel compression (only none/lzma)", note_sz);
        return -1;
    }
    if (fk.comp == 1) {
        u64 out_len = 0;
        if (lzma_decode_alone(fk.data, fk.len, img, img_cap,
                              &out_len, note, note_sz) < 0)
            return -1;
        return parse_arm64(img, out_len, ks, note, note_sz);
    }

    return parse_arm64(fk.data, fk.len, ks, note, note_sz);
}

static int load_raw(long fd, u64 src_size,
                    u8 *img, u64 img_cap,
                    struct ksrc *ks, char *note, int note_sz)
{
    if (src_size > img_cap) {
        scpy(note, "bare Image exceeds img buffer", note_sz);
        return -1;
    }
    if (ksrc_pread(fd, img, src_size, 0) != (long)src_size) {
        scpy(note, "bare Image read failed", note_sz);
        return -1;
    }
    return parse_arm64(img, src_size, ks, note, note_sz);
}

static int load_elf(long fd, u64 src_size, long probe_len,
                    u8 *img, u64 img_cap,
                    u8 *fit, u64 fit_cap,
                    struct ksrc *ks, char *note, int note_sz)
{
    u16 phentsize, phnum;
    u32 phoff;
    int i;

    if (probe_len < 52) {
        scpy(note, "incomplete ELF header", note_sz);
        return -1;
    }
    if (g_probe[4] != 1 || g_probe[5] != 1) {
        scpy(note, "not an ELF32 little-endian container", note_sz);
        return -1;
    }
    phentsize = le16ld(g_probe + 42);
    phnum     = le16ld(g_probe + 44);
    phoff     = le32ld(g_probe + 28);
    if (phentsize != 32 || phnum == 0 || phnum > 64) {
        scpy(note, "abnormal ELF phdr layout", note_sz);
        return -1;
    }
    if ((u64)phoff + (u64)phnum * 32u > (u64)probe_len) {
        scpy(note, "ELF phdr table exceeds probe area", note_sz);
        return -1;
    }

    for (i = 0; i < phnum; i++) {
        const u8 *ph = g_probe + phoff + (u32)i * 32u;
        u32 type   = le32ld(ph);
        u32 off    = le32ld(ph + 4);
        u32 filesz = le32ld(ph + 16);
        u8 shead[64];
        long got;

        if (type != 1) continue;
        if (filesz < 4) continue;
        if ((u64)off + filesz > src_size) continue;

        got = ksrc_pread(fd, shead, sizeof(shead), off);
        if (got < 4) continue;

        if (be32ld(shead) == 0xD00DFEEDU) {
            if (filesz > fit_cap) continue;
            if (ksrc_pread(fd, fit, filesz, off) != (long)filesz) {
                scpy(note, "PT_LOAD segment (FIT) read failed", note_sz);
                return -1;
            }
            return extract_from_fit(fit, filesz, img, img_cap,
                                    ks, note, note_sz);
        }
        if (has_arm64_magic(shead, (u64)got)) {
            if (filesz > img_cap) continue;
            if (ksrc_pread(fd, img, filesz, off) != (long)filesz) {
                scpy(note, "PT_LOAD segment (Image) read failed", note_sz);
                return -1;
            }
            return parse_arm64(img, filesz, ks, note, note_sz);
        }
    }
    scpy(note, "no usable PT_LOAD payload segment in ELF container", note_sz);
    return -1;
}

int container_load_kernel(long fd, u64 src_size,
                          u8 *img, u64 img_cap,
                          u8 *fit, u64 fit_cap,
                          struct ksrc *ks, char *note, int note_sz)
{
    long pl;

    if (src_size == 0) {
        scpy(note, "kernel source is empty", note_sz);
        return -1;
    }
    pl = ksrc_pread(fd, g_probe, PROBE_CAP, 0);
    if (pl <= 0) {
        scpy(note, "kernel source is unreadable", note_sz);
        return -1;
    }

    if (be32ld(g_probe) == 0xD00DFEEDU) {

        if (src_size > fit_cap) {
            scpy(note, "FIT exceeds fit buffer", note_sz);
            return -1;
        }
        if (ksrc_pread(fd, fit, src_size, 0) != (long)src_size) {
            scpy(note, "FIT read failed", note_sz);
            return -1;
        }
        return extract_from_fit(fit, src_size, img, img_cap,
                                ks, note, note_sz);
    }

    if (g_probe[0] == 0x7f && g_probe[1] == 'E' &&
        g_probe[2] == 'L' && g_probe[3] == 'F')
        return load_elf(fd, src_size, pl, img, img_cap, fit, fit_cap,
                        ks, note, note_sz);

    if (has_arm64_magic(g_probe, (u64)pl))
        return load_raw(fd, src_size, img, img_cap, ks, note, note_sz);

    scpy(note, "unrecognized kernel source format (expected FIT/ELF32 container/arm64 Image)", note_sz);
    return -1;
}

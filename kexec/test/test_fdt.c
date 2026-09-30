
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "fdt.h"

static u8 dtb_buf[1u << 20];

int main(int argc, char **argv)
{
    const char *path, *set = 0, *append = 0, *dump = 0;
    int fd, i;
    long n;
    long long flen;
    struct fdt_blob b;
    char note[128] = {0};

    if (argc < 2) {
        fprintf(stderr, "usage: %s <dtb> [--get] [--set <s>] [--append <s>] [--dump <out>]\n",
                argv[0]);
        return 2;
    }
    path = argv[1];
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--get")) {

        } else if (!strcmp(argv[i], "--set") && i + 1 < argc) {
            set = argv[++i];
        } else if (!strcmp(argv[i], "--append") && i + 1 < argc) {
            append = argv[++i];
        } else if (!strcmp(argv[i], "--dump") && i + 1 < argc) {
            dump = argv[++i];
        } else {
            fprintf(stderr, "bad arg: %s\n", argv[i]);
            return 2;
        }
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open dtb"); return 2; }
    flen = lseek(fd, 0, SEEK_END);
    if (flen < 0 || (u64)flen > sizeof(dtb_buf)) {
        fprintf(stderr, "dtb too big\n");
        return 2;
    }
    lseek(fd, 0, SEEK_SET);
    n = read(fd, dtb_buf, flen);
    if (n < 0 || (u64)n != flen) { perror("read"); return 2; }
    close(fd);

    if (fdt_open(&b, dtb_buf, sizeof(dtb_buf)) < 0) {
        fprintf(stderr, "FAIL: fdt_open rejected %s\n", path);
        return 1;
    }
    printf("fdt_open ok: struct@0x%x+%u strings@0x%x+%u end=%llu\n",
           b.off_struct, b.sz_struct, b.off_strings, b.sz_strings,
           (unsigned long long)b.end);

    {
        const u8 *ba;
        u32 bl;
        int r = fdt_chosen_bootargs(&b, &ba, &bl);
        if (r == 0)
            printf("bootargs: [%u] %.*s\n", bl, (int)bl, (const char *)ba);
        else
            printf("bootargs: (absent; chosen node %s)\n",
                   r == 1 ? "exists" : "missing");
    }

    if (set || append) {
        char final[4096];
        final[0] = 0;
        if (set) {
            snprintf(final, sizeof(final), "%s", set);
        } else {
            const u8 *ba;
            u32 bl = 0;
            if (fdt_chosen_bootargs(&b, &ba, &bl) == 0 && bl)
                snprintf(final, sizeof(final), "%.*s %s",
                         (int)bl, (const char *)ba, append);
            else
                snprintf(final, sizeof(final), "%s", append);
        }
        if (fdt_chosen_set_bootargs(&b, final, note, sizeof(note)) < 0) {
            fprintf(stderr, "FAIL: set_bootargs: %s\n", note);
            return 1;
        }
        printf("set ok: [%u] %s\n", (unsigned)slen(final), final);
    }

    if (dump) {
        u64 newlen = fdt_finalize(&b);
        fd = open(dump, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { perror("open dump"); return 2; }
        {
            u64 done = 0;
            while (done < newlen) {
                n = write(fd, dtb_buf + done, newlen - done);
                if (n <= 0) { perror("write"); return 2; }
                done += (u64)n;
            }
        }
        close(fd);
        printf("dumped %llu bytes -> %s\n",
               (unsigned long long)newlen, dump);
    }
    return 0;
}

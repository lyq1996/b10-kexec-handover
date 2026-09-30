
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "container.h"

static u8 img_buf[32u << 20];
static u8 fit_buf[16u << 20];

long ksrc_pread(long fd, u8 *buf, u64 len, u64 off)
{
    return (long)pread((int)fd, buf, (size_t)len, (off_t)off);
}

u64 ksrc_fsize(long fd)
{
    off_t r = lseek((int)fd, 0, SEEK_END);
    return r < 0 ? 0 : (u64)r;
}

int main(int argc, char **argv)
{
    const char *path, *dump = 0;
    int fd, i;
    u64 sz;
    struct ksrc ks;
    char note[128] = {0};

    if (argc < 2) {
        fprintf(stderr, "usage: %s <src> [--dump <out>]\n", argv[0]);
        return 2;
    }
    path = argv[1];
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--dump") && i + 1 < argc)
            dump = argv[++i];
        else {
            fprintf(stderr, "bad arg: %s\n", argv[i]);
            return 2;
        }
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return 2; }
    sz = ksrc_fsize(fd);
    if (sz == 0) {
        fprintf(stderr, "empty source\n");
        return 2;
    }

    if (container_load_kernel(fd, sz, img_buf, sizeof(img_buf),
                              fit_buf, sizeof(fit_buf),
                              &ks, note, sizeof(note)) < 0) {
        fprintf(stderr, "parse rejected: %s\n", note);
        return 1;
    }
    close(fd);

    printf("image_len  = %llu\n", (unsigned long long)ks.image_len);
    printf("text_off   = 0x%llx\n", (unsigned long long)ks.text_off);
    printf("image_size = 0x%llx\n", (unsigned long long)ks.image_size);
    printf("first16    : ");
    for (i = 0; i < 16; i++)
        printf("%02x", ks.image[i]);
    printf("\n");

    if (dump) {
        fd = open(dump, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { perror("open dump"); return 2; }
        {
            u64 done = 0;
            long n;
            while (done < ks.image_len) {
                n = write(fd, ks.image + done, ks.image_len - done);
                if (n <= 0) { perror("write"); return 2; }
                done += (u64)n;
            }
        }
        close(fd);
        printf("dumped %llu bytes -> %s\n",
               (unsigned long long)ks.image_len, dump);
    }
    return 0;
}

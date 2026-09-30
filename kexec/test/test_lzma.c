
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "lzma.h"

static u8 in_buf[40u << 20];
static u8 out_buf[32u << 20];

int main(int argc, char **argv)
{
    const char *in_path, *out_path;
    long expect = -1;
    int fd, i;
    long n;
    u64 in_len, out_len = 0;
    char note[128] = {0};

    if (argc < 3) {
        fprintf(stderr, "usage: %s <in.lzma> <out> [--expect <size>]\n", argv[0]);
        return 2;
    }
    in_path = argv[1];
    out_path = argv[2];
    for (i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--expect") && i + 1 < argc)
            expect = atol(argv[++i]);
        else {
            fprintf(stderr, "bad arg: %s\n", argv[i]);
            return 2;
        }
    }

    fd = open(in_path, O_RDONLY);
    if (fd < 0) { perror("open in"); return 2; }
    n = read(fd, in_buf, sizeof(in_buf));
    if (n < 0) { perror("read"); return 2; }
    close(fd);
    in_len = (u64)n;

    if (lzma_decode_alone(in_buf, in_len, out_buf, sizeof(out_buf),
                          &out_len, note, sizeof(note)) < 0) {
        fprintf(stderr, "decode rejected: %s\n", note);
        return 1;
    }
    printf("decoded %llu bytes\n", (unsigned long long)out_len);
    if (expect >= 0 && (long)out_len != expect) {
        fprintf(stderr, "size mismatch: got %lld expect %ld\n",
                (long long)out_len, expect);
        return 1;
    }

    fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open out"); return 2; }
    {
        u64 done = 0;
        while (done < out_len) {
            n = write(fd, out_buf + done, out_len - done);
            if (n <= 0) { perror("write"); return 2; }
            done += (u64)n;
        }
    }
    close(fd);
    return 0;
}

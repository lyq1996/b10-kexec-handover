
#ifndef KJUMP_CONTAINER_H
#define KJUMP_CONTAINER_H

#include "common.h"

struct ksrc {
    const u8 *image;
    u64 image_len;
    u64 text_off;
    u64 image_size;
};

long ksrc_pread(long fd, u8 *buf, u64 len, u64 off);
u64  ksrc_fsize(long fd);

int container_load_kernel(long fd, u64 src_size,
                          u8 *img, u64 img_cap,
                          u8 *fit, u64 fit_cap,
                          struct ksrc *ks, char *note, int note_sz);

#endif

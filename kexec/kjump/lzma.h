
#ifndef KJUMP_LZMA_H
#define KJUMP_LZMA_H

#include "common.h"

struct lzma_params {
    u32 lc, lp, pb;
    u32 dict;
    u64 size;
};

int lzma_parse_header(const u8 *in, u64 in_len, struct lzma_params *p,
                      char *note, int note_sz);

int lzma_decode_alone(const u8 *in, u64 in_len,
                      u8 *out, u64 out_cap,
                      u64 *out_len,
                      char *note, int note_sz);

#endif

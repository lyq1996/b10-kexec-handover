
#ifndef KJUMP_FDT_H
#define KJUMP_FDT_H

#include "common.h"

struct fdt_blob {
    u8 *buf;
    u64 cap;
    u32 off_struct, sz_struct;
    u32 off_strings, sz_strings;
    u64 end;
};

int fdt_open(struct fdt_blob *b, u8 *buf, u64 cap);

u64 fdt_finalize(struct fdt_blob *b);

int fdt_chosen_bootargs(const struct fdt_blob *b, const u8 **ptr, u32 *len);

int fdt_chosen_set_bootargs(struct fdt_blob *b, const char *args,
                            char *note, int note_sz);

struct fit_kernel {
    const u8 *data;
    u32 len;
    int comp;
    const char *desc;
    u32 desc_len;
    const char *comp_str;
    u32 comp_len;
    u32 n_siblings;
};

int fdt_fit_find_kernel(const struct fdt_blob *b, struct fit_kernel *k);

#endif

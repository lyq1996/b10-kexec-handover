
#include "fdt.h"

#define FDT_BEGIN_NODE 1
#define FDT_END_NODE   2
#define FDT_PROP       3
#define FDT_NOP        4
#define FDT_END        9

#define FDT_HEADER_SIZE 40
#define FDT_MIN_VERSION 17

#define BOOTARGS_MAX 8192

int fdt_open(struct fdt_blob *b, u8 *buf, u64 cap)
{
    u32 magic, tsz, ver;
    u64 s_end, str_end;

    if (cap < FDT_HEADER_SIZE) return -1;
    magic = be32ld(buf);
    tsz   = be32ld(buf + 4);
    ver   = be32ld(buf + 20);
    if (magic != 0xD00DFEEDU) return -1;
    if (tsz < FDT_HEADER_SIZE) return -1;
    if (ver < FDT_MIN_VERSION) return -1;

    b->buf         = buf;
    b->cap         = cap;
    b->off_struct  = be32ld(buf + 8);
    b->off_strings = be32ld(buf + 12);
    b->sz_strings  = be32ld(buf + 32);
    b->sz_struct   = be32ld(buf + 36);

    if (b->off_struct < FDT_HEADER_SIZE) return -1;
    s_end = (u64)b->off_struct + b->sz_struct;
    str_end = (u64)b->off_strings + b->sz_strings;
    if (s_end > cap || str_end > cap) return -1;
    if (b->off_strings < s_end) return -1;

    b->end = s_end > str_end ? s_end : str_end;
    if (b->end < FDT_HEADER_SIZE) b->end = FDT_HEADER_SIZE;
    return 0;
}

u64 fdt_finalize(struct fdt_blob *b)
{
    be32st(b->buf + 4, (u32)b->end);
    return b->end;
}

struct fdt_walk {
    const struct fdt_blob *b;
    u64 pos;
    u64 s_end;
};

enum {
    WALK_OK = 0,
    WALK_END = 1,
    WALK_BAD = -1
};

static int fdt_walk_start(struct fdt_walk *w, const struct fdt_blob *b)
{
    w->b = b;
    w->pos = b->off_struct;
    w->s_end = (u64)b->off_struct + b->sz_struct;
    return 0;
}

static int fdt_walk_next(struct fdt_walk *w, int *tok, const char **name,
                         const u8 **data, u32 *len, u32 *nameoff)
{
    u32 t, v_len, v_nameoff, adv;
    u64 namelen, strp;

    if (w->pos + 4 > w->s_end) return WALK_BAD;
    t = be32ld(w->b->buf + w->pos);
    *tok = (int)t;

    switch (t) {
    case FDT_BEGIN_NODE:

        namelen = 0;
        while (w->pos + 4 + namelen < w->s_end &&
               w->b->buf[w->pos + 4 + namelen] != 0)
            namelen++;
        if (w->pos + 4 + namelen >= w->s_end) return WALK_BAD;
        if (name) *name = (const char *)(w->b->buf + w->pos + 4);
        adv = 4 + align4((u32)namelen + 1);
        if (w->pos + adv > w->s_end) return WALK_BAD;
        w->pos += adv;
        return WALK_OK;

    case FDT_PROP:
        if (w->pos + 12 > w->s_end) return WALK_BAD;
        v_len     = be32ld(w->b->buf + w->pos + 4);
        v_nameoff = be32ld(w->b->buf + w->pos + 8);

        if ((u64)v_len > w->s_end) return WALK_BAD;
        if (w->pos + 12 + align4(v_len) > w->s_end) return WALK_BAD;
        if (v_nameoff >= w->b->sz_strings) return WALK_BAD;

        strp = (u64)w->b->off_strings + v_nameoff;
        while (strp < (u64)w->b->off_strings + w->b->sz_strings &&
               w->b->buf[strp] != 0)
            strp++;
        if (strp >= (u64)w->b->off_strings + w->b->sz_strings) return WALK_BAD;
        if (len) *len = v_len;
        if (nameoff) *nameoff = v_nameoff;
        if (data) *data = w->b->buf + w->pos + 12;
        w->pos += 12 + align4(v_len);
        return WALK_OK;

    case FDT_END_NODE:
    case FDT_NOP:
        w->pos += 4;
        return WALK_OK;

    case FDT_END:
        return WALK_END;

    default:
        return WALK_BAD;
    }
}

static const char *fdt_prop_name(const struct fdt_blob *b, u32 nameoff)
{
    return (const char *)(b->buf + b->off_strings + nameoff);
}

static int name_eq(const char *fdt_name, const char *query)
{
    u64 i;
    for (i = 0; query[i]; i++)
        if (fdt_name[i] != query[i]) return 0;
    return fdt_name[i] == 0 || fdt_name[i] == '@';
}

static int prop_streq(const u8 *data, u32 len, const char *query)
{
    u64 n = slen(query);
    if ((u64)len < n) return 0;
    if (memcmp(data, query, n) != 0) return 0;
    if ((u64)len == n) return 1;
    return data[n] == 0;
}

static int find_chosen(const struct fdt_blob *b,
                       u32 *prop_off, u32 *data_off, u32 *old_len,
                       u32 *body_off)
{
    struct fdt_walk w;
    int tok, in_chosen = 0, seen_chosen = 0, r = -1;
    u64 depth = 0;

    fdt_walk_start(&w, b);
    for (;;) {
        int t;
        const char *nm;
        const u8 *dt;
        u32 plen, pno;
        t = fdt_walk_next(&w, &tok, &nm, &dt, &plen, &pno);
        if (t == WALK_END) break;
        if (t == WALK_BAD) return -2;
        switch (tok) {
        case FDT_BEGIN_NODE:
            depth++;
            if (depth == 2 && !in_chosen && name_eq(nm, "chosen")) {
                in_chosen = 1;
                seen_chosen = 1;
                r = 1;
                *body_off = (u32)w.pos;
            }
            break;
        case FDT_PROP:
            if (in_chosen && depth == 2 &&
                name_eq(fdt_prop_name(b, pno), "bootargs")) {
                *prop_off = (u32)(w.pos - 12 - align4(plen));
                *data_off = (u32)(w.pos - align4(plen));
                *old_len = plen;
                return 0;
            }
            break;
        case FDT_END_NODE:
            if (in_chosen && depth == 2) in_chosen = 0;
            depth--;
            break;
        default:
            break;
        }
    }
    return seen_chosen ? r : -1;
}

int fdt_chosen_bootargs(const struct fdt_blob *b, const u8 **ptr, u32 *len)
{
    u32 prop_off = 0, data_off = 0, old_len = 0, body_off = 0;
    int r = find_chosen(b, &prop_off, &data_off, &old_len, &body_off);
    if (r == 0) {
        *ptr = b->buf + data_off;
        *len = old_len;
    }
    return r;
}

int fdt_chosen_set_bootargs(struct fdt_blob *b, const char *args,
                            char *note, int note_sz)
{
    u32 prop_off = 0, data_off = 0, old_len = 0, body_off = 0;
    u32 need, slot_old, delta;
    int r;

    need = (u32)slen(args) + 1;
    if (need > BOOTARGS_MAX) {
        scpy(note, "bootargs too long", note_sz);
        return -1;
    }

    r = find_chosen(b, &prop_off, &data_off, &old_len, &body_off);
    if (r == -2) { scpy(note, "corrupt dtb structure", note_sz); return -1; }
    if (r == -1) { scpy(note, "dtb has no /chosen node", note_sz); return -1; }

    if (r == 0) {

        slot_old = align4(old_len);
        if (need <= (u64)slot_old) {

            memcpy(b->buf + data_off, args, need);
            memset(b->buf + data_off + need, 0, slot_old - need);
            if (need > old_len)
                be32st(b->buf + prop_off + 4, need);
            return 0;
        }

        delta = align4(need) - slot_old;
        if (b->end + delta > b->cap) {
            scpy(note, "bootargs growth exceeds dtb buffer", note_sz);
            return -1;
        }
        {
            u64 s_end = (u64)b->off_struct + b->sz_struct;

            memmove(b->buf + b->off_strings + delta, b->buf + b->off_strings,
                    b->sz_strings);
            memmove(b->buf + data_off + slot_old + delta,
                    b->buf + data_off + slot_old,
                    s_end - (data_off + slot_old));

            b->off_strings += delta;
            b->sz_struct += delta;
            be32st(b->buf + 12, b->off_strings);
            be32st(b->buf + 36, b->sz_struct);
            b->end += delta;

            be32st(b->buf + prop_off + 4, need);
            memcpy(b->buf + data_off, args, need);
            memset(b->buf + data_off + need, 0, align4(need) - need);
        }
        return 0;
    }

    {
        static const char name_bootargs[] = "bootargs";
        u32 add_len = 12 + align4(need);
        u32 nameoff = 0, extra = 0;
        u64 k, s_end;

        for (k = 0; k + 9 <= b->sz_strings; k++) {
            if ((k == 0 || b->buf[b->off_strings + k - 1] == 0) &&
                memcmp(b->buf + b->off_strings + k, name_bootargs, 9) == 0) {
                nameoff = (u32)k;
                break;
            }
        }
        if (k + 9 > b->sz_strings) {
            nameoff = b->sz_strings;
            extra = align4(9);
        }

        if (b->end + add_len + extra > b->cap) {
            scpy(note, "bootargs insertion exceeds dtb buffer", note_sz);
            return -1;
        }

        s_end = (u64)b->off_struct + b->sz_struct;

        memmove(b->buf + b->off_strings + add_len, b->buf + b->off_strings,
                b->sz_strings);
        if (extra) {
            memcpy(b->buf + b->off_strings + add_len + b->sz_strings,
                   name_bootargs, 9);
            memset(b->buf + b->off_strings + add_len + b->sz_strings + 9,
                   0, extra - 9);
            b->sz_strings += extra;
            be32st(b->buf + 32, b->sz_strings);
        }

        memmove(b->buf + body_off + add_len, b->buf + body_off,
                s_end - body_off);

        be32st(b->buf + body_off, FDT_PROP);
        be32st(b->buf + body_off + 4, need);
        be32st(b->buf + body_off + 8, nameoff);
        memcpy(b->buf + body_off + 12, args, need);
        memset(b->buf + body_off + 12 + need, 0, align4(need) - need);

        b->off_strings += add_len;
        b->sz_struct += add_len;
        be32st(b->buf + 12, b->off_strings);
        be32st(b->buf + 36, b->sz_struct);
        b->end += add_len + extra;
        return 0;
    }
}

int fdt_fit_find_kernel(const struct fdt_blob *b, struct fit_kernel *k)
{
    struct fdt_walk w;
    int tok, in_images = 0, capturing = 0, r = -1;
    u64 depth = 0;
    u32 n_children = 0;
    const u8 *c_type = 0; u32 c_type_len = 0;
    const u8 *c_data = 0; u32 c_data_len = 0;
    const u8 *c_comp = 0; u32 c_comp_len = 0;
    const u8 *c_desc = 0; u32 c_desc_len = 0;

    fdt_walk_start(&w, b);
    for (;;) {
        int t;
        const char *nm;
        const u8 *dt;
        u32 plen, pno;
        t = fdt_walk_next(&w, &tok, &nm, &dt, &plen, &pno);
        if (t == WALK_END) break;
        if (t == WALK_BAD) return -2;
        switch (tok) {
        case FDT_BEGIN_NODE:
            depth++;
            if (depth == 2 && name_eq(nm, "images")) {
                in_images = 1;
            } else if (in_images && depth == 3) {

                capturing = 1;
                n_children++;
                c_type = c_data = c_comp = c_desc = 0;
                c_type_len = c_data_len = c_comp_len = c_desc_len = 0;
            }
            break;
        case FDT_PROP:
            if (capturing && depth == 3) {
                const char *pn = fdt_prop_name(b, pno);
                if (name_eq(pn, "type"))          { c_type = dt; c_type_len = plen; }
                else if (name_eq(pn, "data"))     { c_data = dt; c_data_len = plen; }
                else if (name_eq(pn, "compression")) { c_comp = dt; c_comp_len = plen; }
                else if (name_eq(pn, "description")) { c_desc = dt; c_desc_len = plen; }
            }
            break;
        case FDT_END_NODE:
            if (capturing && depth == 3) {

                capturing = 0;
                if (c_type && c_data &&
                    prop_streq(c_type, c_type_len, "kernel")) {
                    k->data = c_data;
                    k->len = c_data_len;
                    k->desc = (const char *)c_desc;
                    k->desc_len = c_desc_len;
                    k->comp_str = (const char *)c_comp;
                    k->comp_len = c_comp_len;
                    k->n_siblings = n_children;
                    k->comp = 0;
                    if (c_comp) {
                        if (prop_streq(c_comp, c_comp_len, "lzma"))
                            k->comp = 1;
                        else if (prop_streq(c_comp, c_comp_len, "none"))
                            k->comp = 0;
                        else
                            k->comp = -1;
                    }
                    return 0;
                }
            }
            if (in_images && depth == 2) in_images = 0;
            depth--;
            break;
        default:
            break;
        }
    }
    return r;
}

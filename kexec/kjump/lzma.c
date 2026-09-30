
#include "lzma.h"
#include "lzma-sdk/LzmaDec.h"

static u16 g_probs[1846 + (768u << 4)];

static void *sz_alloc(void *ctx, size_t size)
{
    (void)ctx;
    return size <= sizeof(g_probs) ? (void *)g_probs : 0;
}

static void sz_free(void *ctx, void *addr)
{
    (void)ctx;
    (void)addr;
}

static ISzAlloc g_alloc = { sz_alloc, sz_free };

int lzma_parse_header(const u8 *in, u64 in_len, struct lzma_params *p,
                      char *note, int note_sz)
{
    u32 d;

    if (in_len < 13) {
        scpy(note, "LZMA stream shorter than 13-byte header", note_sz);
        return -1;
    }
    d = in[0];
    p->lc = d % 9;
    d /= 9;
    p->lp = d % 5;
    p->pb = d / 5;
    p->dict = le32ld(in + 1);
    p->size = le64ld(in + 5);

    if (p->lc > 4 || p->lp > 4 || p->pb > 4 || p->lc + p->lp > 4) {
        scpy(note, "LZMA parameters exceed policy limits", note_sz);
        return -1;
    }
    if (p->dict == 0) {
        scpy(note, "LZMA dict_size is 0", note_sz);
        return -1;
    }
    return 0;
}

int lzma_decode_alone(const u8 *in, u64 in_len,
                      u8 *out, u64 out_cap,
                      u64 *out_len,
                      char *note, int note_sz)
{
    struct lzma_params hp;
    SizeT dest_len, src_len;
    ELzmaStatus st;
    SRes r;
    int unknown_size;

    if (lzma_parse_header(in, in_len, &hp, note, note_sz) < 0)
        return -1;
    if (in_len < 13 + 5) {
        scpy(note, "LZMA stream missing range decoder init data", note_sz);
        return -1;
    }

    unknown_size = (hp.size == ~(u64)0);
    if (unknown_size) {
        dest_len = (SizeT)out_cap;
    } else {
        if (hp.size > out_cap) {
            scpy(note, "LZMA declared size exceeds output buffer", note_sz);
            return -1;
        }
        dest_len = (SizeT)hp.size;
    }

    src_len = (SizeT)(in_len - 13);
    r = LzmaDecode(out, &dest_len, in + 13, &src_len,
                   in, 5, LZMA_FINISH_END, &st, &g_alloc);
    if (r != SZ_OK) {
        scpy(note, r == SZ_ERROR_INPUT_EOF ? "LZMA input exhausted early"
                                           : "LZMA data corrupt", note_sz);
        return -1;
    }
    if (st == LZMA_STATUS_NEEDS_MORE_INPUT) {
        scpy(note, "LZMA input exhausted early", note_sz);
        return -1;
    }
    if (st == LZMA_STATUS_NOT_FINISHED) {
        scpy(note, "LZMA stream unfinished (output filled first)", note_sz);
        return -1;
    }
    if (unknown_size) {
        if (st != LZMA_STATUS_FINISHED_WITH_MARK) {
            scpy(note, "LZMA unknown-size stream has no end marker", note_sz);
            return -1;
        }
    } else {
        if (st != LZMA_STATUS_FINISHED_WITH_MARK &&
            st != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) {
            scpy(note, "LZMA stream not finished at declared size", note_sz);
            return -1;
        }
        if ((u64)dest_len != hp.size) {
            scpy(note, "LZMA actual output size differs from declared size", note_sz);
            return -1;
        }
    }
    *out_len = (u64)dest_len;
    return 0;
}

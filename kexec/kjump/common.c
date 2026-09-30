
#include "common.h"

static s64 sys6(s64 n, s64 a, s64 b, s64 c, s64 d, s64 e, s64 f)
{
    register s64 x8 __asm__("x8") = n;
    register s64 x0 __asm__("x0") = a;
    register s64 x1 __asm__("x1") = b;
    register s64 x2 __asm__("x2") = c;
    register s64 x3 __asm__("x3") = d;
    register s64 x4 __asm__("x4") = e;
    register s64 x5 __asm__("x5") = f;
    __asm__ volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
                     : "memory", "cc");
    return x0;
}

__attribute__((optimize("-fno-tree-loop-distribute-patterns")))
void *memcpy(void *dest, const void *src, size_t n)
{
    u8 *d = dest;
    const u8 *s = src;
    size_t i;
    for (i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

__attribute__((optimize("-fno-tree-loop-distribute-patterns")))
void *memmove(void *dest, const void *src, size_t n)
{
    u8 *d = dest;
    const u8 *s = src;
    size_t i;
    if (d < s || d >= s + n) {
        for (i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (i = n; i-- > 0; ) d[i] = s[i];
    }
    return dest;
}

__attribute__((optimize("-fno-tree-loop-distribute-patterns")))
void *memset(void *dest, int c, size_t n)
{
    u8 *d = dest;
    size_t i;
    for (i = 0; i < n; i++) d[i] = (u8)c;
    return dest;
}

__attribute__((optimize("-fno-tree-loop-distribute-patterns")))
int memcmp(const void *a, const void *b, size_t n)
{
    const u8 *x = a, *y = b;
    size_t i;
    for (i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

s64 sys_open(const char *path, long flags, long mode)
{
    return sys6(SYS_openat, AT_FDCWD, (s64)path, flags, mode, 0, 0);
}

void sys_close(long fd) { sys6(SYS_close, fd, 0, 0, 0, 0, 0); }

s64 sys_read(long fd, void *buf, u64 len)
{
    return sys6(SYS_read, fd, (s64)buf, (s64)len, 0, 0, 0);
}

s64 sys_write(long fd, const void *buf, u64 len)
{
    return sys6(SYS_write, fd, (s64)buf, (s64)len, 0, 0, 0);
}

s64 sys_lseek(long fd, s64 off, long whence)
{
    return sys6(SYS_lseek, fd, off, whence, 0, 0, 0);
}

s64 sys_getdents64(long fd, void *buf, u64 len)
{
    return sys6(SYS_getdents64, fd, (s64)buf, (s64)len, 0, 0, 0);
}

s64 sys_pwrite64(long fd, const void *buf, u64 len, u64 off)
{
    return sys6(SYS_pwrite64, fd, (s64)buf, (s64)len, (s64)off, 0, 0);
}

s64 sys_ioctl(long fd, long req, void *arg)
{
    return sys6(SYS_ioctl, fd, req, (s64)arg, 0, 0, 0);
}

s64 sys_init_module(void *umod, u64 len, const char *uargs)
{
    return sys6(SYS_init_module, (s64)umod, (s64)len, (s64)uargs, 0, 0, 0);
}

void do_sync(void) { sys6(SYS_sync, 0, 0, 0, 0, 0, 0); }

s64 sys_nanosleep(void *req, void *rem)
{
    return sys6(SYS_nanosleep, (s64)req, (s64)rem, 0, 0, 0, 0);
}

void msleep(long ms)
{

    s64 ts[2] = { ms / 1000, (ms % 1000) * 1000000L };
    sys_nanosleep(ts, 0);
}

void sys_exit(long code)
{
    sys6(SYS_exit_group, code, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

long pread_full(long fd, u8 *buf, u64 len, u64 off)
{
    u64 got = 0;
    while (got < len) {
        s64 n = sys6(SYS_pread64, fd, (s64)(buf + got),
                     (s64)(len - got), (s64)(off + got), 0, 0);
        if (n < 0) return -1;
        if (n == 0) break;
        got += (u64)n;
    }
    return (long)got;
}

u64 file_size(long fd)
{
    s64 r = sys_lseek(fd, 0, SEEK_END);
    return r < 0 ? 0 : (u64)r;
}

int readfile(const char *path, u8 *buf, u64 max, u64 *out_len)
{
    s64 fd = sys_open(path, O_RDONLY, 0);
    u64 total = 0;
    if (fd < 0) return -1;
    for (;;) {
        s64 n;
        if (total >= max) {
            u8 probe;
            n = sys_read(fd, &probe, 1);
            if (n < 0) { sys_close(fd); return -1; }
            if (n > 0) { sys_close(fd); return 1; }
            break;
        }
        n = sys_read(fd, buf + total, max - total);
        if (n < 0) { sys_close(fd); return -1; }
        if (n == 0) break;
        total += (u64)n;
    }
    sys_close(fd);
    *out_len = total;
    return 0;
}

void out(const char *s)
{
    sys_write(2, s, slen(s));
}

void out_hex(const char *tag, u64 v)
{
    static char buf[32];
    u64 i = 0, j;
    out(tag);
    buf[i++] = '0'; buf[i++] = 'x';
    if (!v) buf[i++] = '0';
    for (j = 0; j < 16 && (v >> ((15 - j) * 4)); j++)
        ;
    for (; j < 16; j++) {
        u64 d = (v >> ((15 - j) * 4)) & 0xF;
        buf[i++] = d < 10 ? (char)('0' + d) : (char)('a' + d - 10);
    }
    buf[i++] = '\n';
    sys_write(2, buf, i);
}

static s64 g_logfd = -1;

void klog_open(const char *path)
{
    g_logfd = sys_open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
}

void klog(const char *s)
{
    if (g_logfd < 0) return;
    sys_write(g_logfd, s, slen(s));
    sys_write(g_logfd, "\n", 1);
    do_sync();
}

void klog_raw(const char *s)
{
    if (g_logfd < 0) return;
    sys_write(g_logfd, s, slen(s));
    do_sync();
}

void klog_hex(const char *tag, u64 v)
{
    char buf[64];
    u64 i = 0, j;
    if (g_logfd < 0) return;
    for (; tag[i] && i < 40; i++) buf[i] = tag[i];
    buf[i++] = '0'; buf[i++] = 'x';
    if (!v) buf[i++] = '0';
    for (j = 0; j < 16 && (v >> ((15 - j) * 4)); j++)
        ;
    for (; j < 16; j++) {
        u64 d = (v >> ((15 - j) * 4)) & 0xF;
        buf[i++] = d < 10 ? (char)('0' + d) : (char)('a' + d - 10);
    }
    buf[i++] = '\n';
    sys_write(g_logfd, buf, i);
    do_sync();
}

long ksrc_pread(long fd, u8 *buf, u64 len, u64 off)
{
    return pread_full(fd, buf, len, off);
}

u64 ksrc_fsize(long fd)
{
    return file_size(fd);
}


#ifndef KJUMP_COMMON_H
#define KJUMP_COMMON_H

#include <stddef.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef long long          s64;

#define SYS_openat      56
#define SYS_close       57
#define SYS_lseek       62
#define SYS_read        63
#define SYS_write       64
#define SYS_pread64     67
#define SYS_pwrite64    68
#define SYS_ioctl       29
#define SYS_getdents64  61
#define SYS_init_module 105
#define SYS_sync        81
#define SYS_exit_group  94
#define SYS_nanosleep   101

#ifndef AT_FDCWD
#define AT_FDCWD   (-100L)
#endif
#ifndef O_RDONLY
#define O_RDONLY   0
#endif
#ifndef O_WRONLY
#define O_WRONLY   1
#endif
#ifndef O_RDWR
#define O_RDWR     2
#endif
#ifndef O_CREAT
#define O_CREAT    0100
#endif
#ifndef O_APPEND
#define O_APPEND   02000
#endif
#ifndef O_TRUNC
#define O_TRUNC    01000
#endif
#ifndef SEEK_END
#define SEEK_END   2
#endif

#ifndef EEXIST
#define EEXIST    17
#endif
#ifndef ENOTTY
#define ENOTTY    25
#endif

static inline u32 align4(u32 x) { return (x + 3u) & ~3u; }

static inline u64 slen(const char *s)
{
    u64 n = 0;
    while (s && s[n]) n++;
    return n;
}

static inline u32 be32ld(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline void be32st(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v;
}

static inline u32 le32ld(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static inline u16 le16ld(const u8 *p)
{
    return (u16)(p[0] | ((u16)p[1] << 8));
}

static inline u64 le64ld(const u8 *p)
{
    return (u64)le32ld(p) | ((u64)le32ld(p + 4) << 32);
}

static inline u64 scpy(char *dst, const char *src, u64 max)
{
    u64 i = 0;
    while (src && src[i] && i + 1 < max) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return i;
}

void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);
int   memcmp(const void *a, const void *b, size_t n);

void out(const char *s);
void out_hex(const char *tag, u64 v);
void klog_open(const char *path);
void klog(const char *s);
void klog_raw(const char *s);
void klog_hex(const char *tag, u64 v);

s64  sys_open(const char *path, long flags, long mode);
void sys_close(long fd);
s64  sys_read(long fd, void *buf, u64 len);
s64  sys_write(long fd, const void *buf, u64 len);
s64  sys_lseek(long fd, s64 off, long whence);
s64  sys_getdents64(long fd, void *buf, u64 len);
s64  sys_pwrite64(long fd, const void *buf, u64 len, u64 off);
s64  sys_ioctl(long fd, long req, void *arg);
s64  sys_init_module(void *umod, u64 len, const char *uargs);
s64  sys_nanosleep(void *req, void *rem);
void do_sync(void);
void sys_exit(long code) __attribute__((noreturn));

void msleep(long ms);

long pread_full(long fd, u8 *buf, u64 len, u64 off);
u64  file_size(long fd);

int readfile(const char *path, u8 *buf, u64 max, u64 *out_len);

#endif

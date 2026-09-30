
#ifndef KEXEC_LITE_ABI_H
#define KEXEC_LITE_ABI_H

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/ioctl.h>

typedef __u64 kl_u64;

#define KL_IOW(t, n, s)     _IOW(t, n, s)
#define KL_IOR(t, n, s)     _IOR(t, n, s)
#define KL_IONONE(t, n)     _IO(t, n)

#else

typedef unsigned long long kl_u64;

#define KL_IOC_NRBITS    8
#define KL_IOC_TYPEBITS  8
#define KL_IOC_SIZEBITS  14
#define KL_IOC_NRSHIFT   0
#define KL_IOC_TYPESHIFT (KL_IOC_NRSHIFT + KL_IOC_NRBITS)
#define KL_IOC_SIZESHIFT (KL_IOC_TYPESHIFT + KL_IOC_TYPEBITS)
#define KL_IOC_DIRSHIFT  (KL_IOC_SIZESHIFT + KL_IOC_SIZEBITS)
#define KL_IOC_NONE      0U
#define KL_IOC_WRITE     1U
#define KL_IOC_READ      2U

#define KL_IOC(dir, type, nr, size)                     \
    (((dir)  << KL_IOC_DIRSHIFT)  |                     \
     ((type) << KL_IOC_TYPESHIFT) |                     \
     ((nr)   << KL_IOC_NRSHIFT)   |                     \
     ((size) << KL_IOC_SIZESHIFT))

#define KL_IOW(t, n, s)     KL_IOC(KL_IOC_WRITE, (t), (n), sizeof(s))
#define KL_IOR(t, n, s)     KL_IOC(KL_IOC_READ,  (t), (n), sizeof(s))
#define KL_IONONE(t, n)     KL_IOC(KL_IOC_NONE,  (t), (n), 0U)

#endif

#define KL_ABI_VERSION  1
#define KL_MAGIC        0xEB

struct kl_load {
    kl_u64 img_ptr;
    kl_u64 img_len;
    kl_u64 dtb_ptr;
    kl_u64 dtb_len;
    kl_u64 mem_base;
    kl_u64 mem_size;
};

struct kl_abi_info {
    kl_u64 version;
    kl_u64 load_size;
};

#define KL_IOC_LOAD     KL_IOW(KL_MAGIC, 1, struct kl_load)
#define KL_IOC_JUMP     KL_IONONE(KL_MAGIC, 2)
#define KL_IOC_VERSION  KL_IOR(KL_MAGIC, 3, struct kl_abi_info)

#define KL_SA_CAT_(a, b) a##b
#define KL_SA_CAT(a, b)  KL_SA_CAT_(a, b)
#define KL_STATIC_ASSERT(cond, tag) \
    typedef char KL_SA_CAT(kl_sa_, tag)[(cond) ? 1 : -1] __attribute__((unused))

KL_STATIC_ASSERT(sizeof(struct kl_load) == 48, kl_load_size);
KL_STATIC_ASSERT(sizeof(struct kl_abi_info) == 16, kl_abi_info_size);

#endif

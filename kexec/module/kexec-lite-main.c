

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/errno.h>
#include <linux/printk.h>

#include "kexec-lite.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("self-contained kexec (no CONFIG_KEXEC needed)");
MODULE_VERSION("1.0");

#define KL_IMG_MAGIC    0x644d5241U

#define KL_CHUNK_ORDER  10
#define KL_CHUNK_SZ     (1UL << (KL_CHUNK_ORDER + PAGE_SHIFT))
#define KL_MAX_CHUNKS   48

#define KL_DTB_SLOT	(128UL << 10)
#define KL_TAIL_SZ	(KL_DTB_SLOT + 0x4000)

extern const char kl_tramp_start[], kl_tramp_end[];

static u64 g_run_base, g_run_size;
static int g_run_chunks;
static u64 g_t0_pa, g_tramp_pa, g_dtb_pa, g_entry_pa;
static int g_loaded;

struct arm64_hdr {
	u32 code0;
	u32 code1;
	u64 text_offset;
	u64 image_size;
	u64 flags;
	u64 res2, res3, res4;
	u32 magic;
	u32 res5;
};
#define HDR_SZ 64
#define HDR_OFF_MAGIC		0x38

static u64 kl_align_up(u64 v, u64 a) { return (v + a - 1) & ~(a - 1); }

static void kl_dcache_clean(void *va, u64 len)
{
	u64 ctr, line, cur, end;
	asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
	line = 16ULL << ((ctr >> 16) & 0xF);
	end = (u64)va + len;
	for (cur = (u64)va & ~(line - 1); cur < end; cur += line)
		asm volatile("dc cvac, %0" :: "r"(cur));
	asm volatile("dsb sy" ::: "memory");
}

static void kl_free_run(void)
{
	if (g_run_chunks) {
		int i;
		for (i = 0; i < g_run_chunks; i++)
			free_pages((unsigned long)phys_to_virt(
				g_run_base + (u64)i * KL_CHUNK_SZ),
				   KL_CHUNK_ORDER);
		g_run_chunks = 0;
	}
	g_loaded = 0;
}

static int kl_grab_run(int k, u64 mem_base, u64 mem_end, u64 *out_base)
{
	struct page *chunks[KL_MAX_CHUNKS];
	struct page *held[KL_MAX_CHUNKS];
	u64 pfns[KL_MAX_CHUNKS];
	int i, j, n = 0, nheld = 0, best = -1, best_len = 0;

	for (i = 0; i < KL_MAX_CHUNKS; i++) {
		struct page *p = alloc_pages(GFP_KERNEL | __GFP_NORETRY,
					     KL_CHUNK_ORDER);
		unsigned long pfn;
		if (!p)
			break;
		pfn = page_to_pfn(p);
		if (((u64)pfn << PAGE_SHIFT) < mem_base ||
		    (((u64)pfn << PAGE_SHIFT) + KL_CHUNK_SZ) > mem_end) {

			if (nheld >= KL_MAX_CHUNKS) {
				__free_pages(p, KL_CHUNK_ORDER);
				break;
			}
			held[nheld++] = p;
			continue;
		}
		chunks[n] = p;
		pfns[n] = (u64)pfn;
		n++;
	}
	for (j = 0; j < nheld; j++)
		__free_pages(held[j], KL_CHUNK_ORDER);

	for (i = 1; i < n; i++) {
		u64 p = pfns[i];
		struct page *c = chunks[i];
		for (j = i - 1; j >= 0 && pfns[j] > p; j--) {
			pfns[j + 1] = pfns[j];
			chunks[j + 1] = chunks[j];
		}
		pfns[j + 1] = p;
		chunks[j + 1] = c;
	}

	for (i = 0; i < n; i++) {
		int len = 1;
		while (i + len < n &&
		       pfns[i + len] == pfns[i] +
		       (u64)len * (KL_CHUNK_SZ >> PAGE_SHIFT))
			len++;
		if (len > best_len) {
			best_len = len;
			best = i;
			i += len - 1;
		}
	}
	if (best_len >= k && best >= 0) {
		*out_base = pfns[best] << PAGE_SHIFT;
		for (j = 0; j < n; j++)
			if (j < best || j >= best + k)
				__free_pages(chunks[j], KL_CHUNK_ORDER);
		g_run_base = *out_base;
		g_run_chunks = k;
		return 0;
	}
	for (j = 0; j < n; j++)
		__free_pages(chunks[j], KL_CHUNK_ORDER);
	return -ENOMEM;
}

static long kl_do_load(struct kl_load __user *arg)
{
	struct kl_load k;
	struct arm64_hdr hdr;
	u64 total, text_off, img_sz, tail, base = 0;
	int kchunks;

	if (copy_from_user(&k, arg, sizeof(k)))
		return -EFAULT;
	if (!k.img_ptr || k.img_len < HDR_SZ || !k.dtb_ptr || !k.dtb_len)
		return -EINVAL;
	if (k.dtb_len > KL_DTB_SLOT)
		return -EINVAL;
	if (copy_from_user(&hdr, (void __user *)(unsigned long)k.img_ptr, HDR_SZ))
		return -EFAULT;
	if (hdr.magic != KL_IMG_MAGIC)
		return -EINVAL;
	text_off = hdr.text_offset;
	img_sz = hdr.image_size;

	if (text_off > (2UL << 20) || (text_off & 0xFFF))
		return -EINVAL;
	if (k.img_len > img_sz)
		img_sz = k.img_len;
	if (k.mem_base < (1UL << 30))
		return -EINVAL;

	kl_free_run();

	total = kl_align_up(text_off + img_sz + KL_TAIL_SZ, 2UL << 20);
	kchunks = (int)((total + KL_CHUNK_SZ - 1) >> (KL_CHUNK_ORDER + PAGE_SHIFT));

	if (kl_grab_run(kchunks, k.mem_base, k.mem_base + k.mem_size, &base))
		return -ENOMEM;
	g_run_size = total;

	tail = base + total - KL_TAIL_SZ;
	g_dtb_pa = tail;
	g_t0_pa  = tail + KL_DTB_SLOT;
	g_tramp_pa = tail + KL_DTB_SLOT + 0x3000;
	g_entry_pa = base + text_off;

	if (copy_from_user(phys_to_virt(base + text_off),
			   (void __user *)(unsigned long)k.img_ptr, k.img_len))
		goto err;
	if (copy_from_user(phys_to_virt(g_dtb_pa),
			   (void __user *)(unsigned long)k.dtb_ptr, k.dtb_len))
		goto err;

	{
		u64 t1_pa = tail + KL_DTB_SLOT + 0x1000,
		    t2_pa = tail + KL_DTB_SLOT + 0x2000;
		u64 *t0 = phys_to_virt(g_t0_pa);
		u64 *t1 = phys_to_virt(t1_pa);
		u64 *t2 = phys_to_virt(t2_pa);
		u64 i0 = (g_tramp_pa >> 39) & 0x1FF;
		u64 i1 = (g_tramp_pa >> 30) & 0x1FF;
		u64 i2 = (g_tramp_pa >> 21) & 0x1FF;

		memset(t0, 0, 0x1000); memset(t1, 0, 0x1000); memset(t2, 0, 0x1000);
		if (i0 == i1)
			goto err;
		t0[i0] = t1_pa | 0x3;
		t0[i1] = t2_pa | 0x3;
		t1[i1] = t2_pa | 0x3;

		t2[i2] = (g_tramp_pa & ~((2UL << 20) - 1)) | (4UL << 2) | (3UL << 8)
			 | (1UL << 10) | 0x1;
	}

	{
		u64 tsz = kl_tramp_end - kl_tramp_start;
		if (tsz > 0x1000)
			goto err;
		memcpy(phys_to_virt(g_tramp_pa), kl_tramp_start, tsz);
		kl_dcache_clean(phys_to_virt(base), total);
	}

	g_loaded = 1;
	pr_info("kexec-lite: loaded run=[%llx+%llx) entry=%llx dtb=%llx tramp=%llx\n",
		base, total, g_entry_pa, g_dtb_pa, g_tramp_pa);
	return 0;
err:
	kl_free_run();
	return -EFAULT;
}

static void __noclone kl_jump(u64 ttbr, u64 tramp, u64 dtb_pa, u64 entry_pa)
{
	asm volatile(
		"msr	daifset, #0xf\n"
		"mrs	x9, CurrentEL\n"
		"cmp	x9, #(2 << 2)\n"
		"b.eq	3f\n"

		"mrs	x2, tcr_el1\n"
		"bic	x2, x2, #(1 << 7)\n"
		"msr	tcr_el1, x2\n"
		"isb\n"
		"msr	ttbr0_el1, %2\n"
		"isb\n"
		"tlbi	vmalle1\n"
		"dsb	nsh\n"
		"isb\n"
		"ic	iallu\n"
		"dsb	nsh\n"
		"isb\n"
		"mov	x20, %0\n"
		"mov	x21, %1\n"
		"br	%3\n"

		"3:	mov	x20, %0\n"
		"mov	x21, %1\n"
		"br	%3\n"
		: : "r"(dtb_pa), "r"(entry_pa), "r"(ttbr), "r"(tramp)
		: "x2", "x9", "x20", "x21", "cc", "memory");
	unreachable();
}

static long kl_do_jump(void)
{
	u64 el, tcr, mair;

	u64 t0v0, t0v1, tv;
	u64 t2v;
	if (!g_loaded)
		return -ENODEV;

	if (num_online_cpus() != 1) {
		pr_info("kexec-lite: refusing jump: %d cpus online (need 1)\n",
			num_online_cpus());
		return -EBUSY;
	}
	t0v0 = *(u64 *)phys_to_virt(g_t0_pa);
	t0v1 = *(u64 *)phys_to_virt(g_t0_pa + 8);
	t2v  = *(u64 *)phys_to_virt(g_t0_pa + 0x2000 +
				    (((g_tramp_pa >> 21) & 0x1FF) << 3));
	tv   = *(u64 *)phys_to_virt(g_tramp_pa);
	asm volatile("mrs %0, CurrentEL\n"
		     "mrs %1, tcr_el1\n"
		     "mrs %2, mair_el1" : "=r"(el), "=r"(tcr), "=r"(mair));
	pr_info("kexec-lite: jumpdbg EL%llu TCR=%llx T0SZ=%llu MAIR=%llx a4=%llx\n",
		el >> 2, tcr, 64 - (tcr & 0x3F), mair, (mair >> 32) & 0xFF);
	pr_info("kexec-lite: jumpdbg T0[0]=%llx T0[1]=%llx T2[i2]=%llx tramp8=%llx\n",
		t0v0, t0v1, t2v, tv);
	pr_info("kexec-lite: jumping (EL%llu) entry=%llx ttbr=%llx\n",
		el >> 2, g_entry_pa, g_t0_pa);

	kl_jump(g_t0_pa, g_tramp_pa, g_dtb_pa, g_entry_pa);
	return -EIO;
}

static long kl_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case KL_IOC_LOAD:
		return kl_do_load((struct kl_load __user *)arg);
	case KL_IOC_JUMP:
		return kl_do_jump();
	case KL_IOC_VERSION: {
		struct kl_abi_info vi = { KL_ABI_VERSION, sizeof(struct kl_load) };
		if (copy_to_user((void __user *)arg, &vi, sizeof(vi)))
			return -EFAULT;
		return 0;
	}
	}
	return -ENOTTY;
}

static const struct file_operations kl_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= kl_ioctl,
};

static struct miscdevice kl_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "kexec-lite",
	.fops	= &kl_fops,
	.mode	= 0600,
};

static int __init kl_init(void)
{
	int r = misc_register(&kl_misc);
	if (r)
		return r;
	pr_info("kexec-lite: ready (/dev/kexec-lite), ABI v%d load=%zu bytes\n",
		KL_ABI_VERSION, sizeof(struct kl_load));
	return 0;
}

static void __exit kl_exit(void)
{
	kl_free_run();
	misc_deregister(&kl_misc);
}

module_init(kl_init);
module_exit(kl_exit);

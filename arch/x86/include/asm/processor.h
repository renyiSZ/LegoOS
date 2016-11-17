/*
 * Copyright (c) 2016 Wuklab, Purdue University. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _ASM_X86_PROCESSOR_H_
#define _ASM_X86_PROCESSOR_H_

#include <asm/thread_info.h>
#include <asm/processor-flags.h>
#include <asm/processor-features.h>

#include <lego/kernel.h>

/**
 * struct cpu_info
 *
 * Everything you want to know about your CPU.
 * It is filled at early boot stage.
 */
struct cpu_info {
	__u8			x86;		/* CPU family */
	__u8			x86_vendor;	/* CPU vendor */
	__u8			x86_model;
	__u8			x86_mask;
	/* Number of 4K pages in DTLB/ITLB combined(in pages): */
	int			x86_tlbsize;
	__u8			x86_virt_bits;
	__u8			x86_phys_bits;
	/* CPUID returned core id bits: */
	__u8			x86_coreid_bits;
	/* Max extended CPUID function supported: */
	__u32			extended_cpuid_level;
	/* Maximum supported CPUID level, -1=no CPUID: */
	int			cpuid_level;
	__u32			x86_capability[NCAPINTS + NBUGINTS];
	char			x86_vendor_id[16];
	char			x86_model_id[64];
	/* in KB - valid for CPUS which support this call: */
	int			x86_cache_size;
	int			x86_cache_alignment;	/* In bytes */
	/* Cache QoS architectural values: */
	int			x86_cache_max_rmid;	/* max index */
	int			x86_cache_occ_scale;	/* scale to bytes */
	int			x86_power;
	unsigned long		loops_per_jiffy;
	/* cpuid returned max cores value: */
	u16			 x86_max_cores;
	u16			apicid;
	u16			initial_apicid;
	u16			x86_clflush_size;
	/* number of cores as seen by the OS: */
	u16			booted_cores;
	/* Physical processor id: */
	u16			phys_proc_id;
	/* Logical processor id: */
	u16			logical_proc_id;
	/* Core id: */
	u16			cpu_core_id;
	/* Index into per_cpu list: */
	u16			cpu_index;
};

#define X86_VENDOR_INTEL	0
#define X86_VENDOR_CYRIX	1
#define X86_VENDOR_AMD		2
#define X86_VENDOR_UMC		3
#define X86_VENDOR_CENTAUR	5
#define X86_VENDOR_TRANSMETA	7
#define X86_VENDOR_NSC		8
#define X86_VENDOR_NUM		9

#define X86_VENDOR_UNKNOWN	0xff

/*
 * X86 Vendor Hooks
 * - Intel (suppoted)
 * - and many others (no)
 */
struct cpu_vendor {
	const char	*c_vendor;
	/* some have two possibilities for cpuid string */
	const char	*c_ident[2];
	void            (*c_early_init)(struct cpu_info *);
	void		(*c_bsp_init)(struct cpu_info *);
	void		(*c_init)(struct cpu_info *);
	void		(*c_identify)(struct cpu_info *);
	void		(*c_detect_tlb)(struct cpu_info *);
	void		(*c_bsp_resume)(struct cpu_info *);
	int		c_x86_vendor;
};

#define cpu_vendor_register(cpu_vendorX) \
	static const struct cpu_vendor *const __cpu_vendor_##cpu_vendorX __used \
	__attribute__((__section__(".x86_cpu_vendor.init"))) = \
	&cpu_vendorX;

extern const struct cpu_vendor *const __x86_cpu_vendor_start[];
extern const struct cpu_vendor *const __x86_cpu_vendor_end[];

/*
 * x86-64 hardware TSS structure
 */
struct x86_hw_tss {
	u32			reserved1;
	u64			sp0;
	u64			sp1;
	u64			sp2;
	u64			reserved2;
	u64			ist[7];
	u32			reserved3;
	u32			reserved4;
	u16			reserved5;
	u16			io_bitmap_base;
} __packed ____cacheline_aligned;

/* IO-bitmap sizes: */
#define IO_BITMAP_BITS			65536
#define IO_BITMAP_BYTES			(IO_BITMAP_BITS/8)
#define IO_BITMAP_LONGS			(IO_BITMAP_BYTES/sizeof(long))
#define IO_BITMAP_OFFSET		offsetof(struct tss_struct, io_bitmap)
#define INVALID_IO_BITMAP_OFFSET	0x8000

struct tss_struct {
	/*
	 * The hardware state:
	 */
	struct x86_hw_tss	x86_tss;

	/*
	 * The extra 1 is there because the CPU will access an
	 * additional byte beyond the end of the IO permission
	 * bitmap. The extra byte must be all 1 bits, and must
	 * be within the limit.
	 */
	unsigned long		io_bitmap[IO_BITMAP_LONGS + 1];
} ____cacheline_aligned;

extern struct tss_struct cpu_tss;

typedef struct {
	unsigned long		seg;
} mm_segment_t;

struct thread_struct {
	unsigned long		sp0;
	unsigned long		sp;
	unsigned short		es;
	unsigned short		ds;
	unsigned short		fsindex;
	unsigned short		gsindex;
	unsigned int		status;
	unsigned long		fsbase;
	unsigned long		gsbase;

	/* Fault info: */
	unsigned long		cr2;
	unsigned long		trap_nr;
	unsigned long		error_code;

	/* IO permissions: */
	unsigned long		*io_bitmap_ptr;
	unsigned long		iopl;
	/* Max allowed port in the bitmap, in bytes: */
	unsigned		io_bitmap_max;

	mm_segment_t		addr_limit;
};

static inline void load_sp0(struct tss_struct *tss,
			    struct thread_struct *thread)
{
	tss->x86_tss.sp0 = thread->sp0;
}

static inline unsigned long current_top_of_stack(void)
{
	return cpu_tss.x86_tss.sp0;
}

/*
 * TOP_OF_KERNEL_STACK_PADDING is a number of unused bytes that we
 * reserve at the top of the kernel stack.  We do it because of a nasty
 * 32-bit corner case.  On x86_32, the hardware stack frame is
 * variable-length.  Except for vm86 mode, struct pt_regs assumes a
 * maximum-length frame.  If we enter from CPL 0, the top 8 bytes of
 * pt_regs don't actually exist.  Ordinarily this doesn't matter, but it
 * does in at least one case:
 *
 * If we take an NMI early enough in SYSENTER, then we can end up with
 * pt_regs that extends above sp0.  On the way out, in the espfix code,
 * we can read the saved SS value, but that value will be above sp0.
 * Without this offset, that can result in a page fault.  (We are
 * careful that, in this case, the value we read doesn't matter.)
 *
 * In vm86 mode, the hardware frame is much longer still, so add 16
 * bytes to make room for the real-mode segments.
 *
 * x86_64 has a fixed-length stack frame.
 */
#ifdef CONFIG_X86_32
# ifdef CONFIG_VM86
#  define TOP_OF_KERNEL_STACK_PADDING 16
# else
#  define TOP_OF_KERNEL_STACK_PADDING 8
# endif
#else
# define TOP_OF_KERNEL_STACK_PADDING 0
#endif

#define TOP_OF_INIT_STACK ((unsigned long)&init_stack + sizeof(init_stack) - \
			   TOP_OF_KERNEL_STACK_PADDING)

/*
 * User space process size. 47bits minus one guard page.  The guard
 * page is necessary on Intel CPUs: if a SYSCALL instruction is at
 * the highest possible canonical userspace address, then that
 * syscall will enter the kernel with a non-canonical return
 * address, and SYSRET will explode dangerously.  We avoid this
 * particular problem by preventing anything from being mapped
 * at the maximum canonical address.
 */
#define TASK_SIZE	((1UL << 47) - PAGE_SIZE)
#define STACK_TOP	TASK_SIZE

#define INIT_THREAD  {				\
	.sp0		= TOP_OF_INIT_STACK,	\
	.addr_limit	= KERNEL_DS,		\
}

#define task_pt_regs(tsk)	((struct pt_regs *)(tsk)->thread.sp0 - 1)

void cpu_init(void);

#endif /* _ASM_X86_PROCESSOR_H_ */
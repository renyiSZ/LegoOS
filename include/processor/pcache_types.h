/*
 * Copyright (c) 2016-2018 Wuklab, Purdue University. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _LEGO_PROCESSOR_PCACHE_TYPES_H_
#define _LEGO_PROCESSOR_PCACHE_TYPES_H_

#include <lego/mm.h>
#include <lego/const.h>
#include <lego/bitops.h>
#include <lego/spinlock.h>

#include <processor/pcache_config.h>

struct pcache_meta;

#define PCACHE_META_SIZE		(sizeof(struct pcache_meta))

enum pcache_set_stat_item {
	PSET_ALLOC,
	PSET_FILL_MEMORY,
	PSET_FILL_VICTIM,
	PSET_EVICTION,

	NR_PSET_STAT_ITEMS
};

#ifdef CONFIG_PCACHE_EVICTION_PERSET_LIST
struct pset_eviction_entry {
	unsigned long		address;	/* page aligned */
	struct task_struct	*owner;
	struct pcache_meta	*pcm;		/* associated pcm */
	struct list_head	next;
};
#endif

struct pset_padding {
	char x[0];
} ____cacheline_aligned_in_smp;
#define PSET_PADDING(name)	struct pset_padding name;

/**
 * struct pcache_set	- Metadata for each cache set
 * @lock: protect (de-)allocation of all ways within this set
 *        protect rmap operations against all ways within this set
 *
 * FAT NOTE:
 * If you add anything here, do not forget to check if this
 * new field needs to be initialized in init_pcache_set_map().
 */
struct pcache_set {
	atomic_t		stat[NR_PSET_STAT_ITEMS];
	unsigned long		flags;

	/*
	 * Eviction Algorithms Specific
	 */

#ifdef CONFIG_PCACHE_EVICT_LRU
	struct list_head	lru_list;
	atomic_t		nr_lru;

	PSET_PADDING(_pad_lru_lock)
	spinlock_t		lru_lock;
#endif

	/*
	 * Eviction Mechanism Specific
	 */

#ifdef CONFIG_PCACHE_EVICTION_VICTIM
	/*
	 * Number of pcache lines in this set that are currently
	 * living in the victim cache. Updated by victim code.
	 *
	 * Used by pgfault to have a quick check.
	 */
	atomic_t		nr_victims;

#elif defined (CONFIG_PCACHE_EVICTION_PERSET_LIST)
	PSET_PADDING(_pad2_)
	spinlock_t		eviction_list_lock;
	struct list_head	eviction_list;
	atomic_t		nr_eviction_entries;
#endif
} ____cacheline_aligned;

static inline void lock_pset(struct pcache_set *pset)
{
#ifdef CONFIG_PCACHE_EVICT_LRU
	spin_lock(&pset->lru_lock);
#elif defined (CONFIG_PCACHE_EVICTION_PERSET_LIST)
	spin_lock(&pset->eviction_list_lock);
#endif
}

static inline void unlock_pset(struct pcache_set *pset)
{
#ifdef CONFIG_PCACHE_EVICT_LRU
	spin_unlock(&pset->lru_lock);
#elif defined (CONFIG_PCACHE_EVICTION_PERSET_LIST)
	spin_unlock(&pset->eviction_list_lock);
#endif
}

struct pcm_pad {
	char x[0];
} ____cacheline_aligned_in_smp;
#define PCM_PADDING(name)	struct pcm_pad name;

/**
 * struct pcache_meta	- Metadata about one pcache line
 * @bits: various state bits (see below)
 * @rmap: reverse mapping info
 * @mapcount: count of ptes mapped to this pcm
 *
 * You can think this structure as the traditional metadata
 * part for a cache line, but with some addtional fields. And
 * this structure is *CPU cacheline size* aligned to minimize
 * CPU cacheline pingpong between different cores.
 *
 * FAT NOTE:
 * If you add anything here, do not forget to check if this
 * new field needs to be initialized in init_pcache_meta_map().
 */
struct pcache_meta {
	unsigned long		bits;
	atomic_t		mapcount;
	atomic_t		_refcount;

	PCM_PADDING(_pad1_)
	struct list_head	rmap;

#ifdef CONFIG_DEBUG_PCACHE
	struct task_struct	*locker;
#endif

#ifdef CONFIG_PCACHE_EVICT_LRU
	struct list_head	lru;
#endif
} ____cacheline_aligned;

enum rmap_caller {
	RMAP_FILL_PAGE_REMOTE,
	RMAP_ZEROFILL,
	RMAP_VICTIM_FILL,
	RMAP_COW,
	RMAP_FORK,
	RMAP_MREMAP_SLOWPATH,

	NR_RMAP_CALLER,
};

struct pcache_rmap {
	pte_t			*page_table;
	unsigned long		flags;
	struct mm_struct	*owner_mm;
	struct task_struct	*owner_process;
	enum rmap_caller	caller;

	/* page aligned */
	unsigned long		address;
	struct list_head	next;
} ____cacheline_aligned;

/*
 * struct pcache_rmap flags
 */

enum pcache_rmap_flags {
	PCACHE_RMAP_reserved,

	NR_PCACHE_RMAP_FLAGS
};

#define TEST_RMAP_FLAGS(uname, lname)				\
static inline int Rmap##uname(const struct pcache_rmap *p)	\
{								\
	return test_bit(PCACHE_RMAP_##lname, &p->flags);	\
}

#define SET_RMAP_FLAGS(uname, lname)				\
static inline void SetRmap##uname(struct pcache_rmap *p)	\
{								\
	set_bit(PCACHE_RMAP_##lname, &p->flags);		\
}

#define CLEAR_RMAP_FLAGS(uname, lname)				\
static inline void ClearRmap##uname(struct pcache_rmap *p)	\
{								\
	clear_bit(PCACHE_RMAP_##lname, &p->flags);		\
}

#define RMAP_FLAGS(uname, lname)				\
	TEST_RMAP_FLAGS(uname, lname)				\
	SET_RMAP_FLAGS(uname, lname)				\
	CLEAR_RMAP_FLAGS(uname, lname)

RMAP_FLAGS(Reserved, reserved)

/*
 * struct pcache_set flags
 */

enum pcache_set_flags {
	PCACHE_SET_evicting,		/* pset is under eviction now */
	PCACHE_SET_sweeping,		/* Sweep thread is scaning this set now */

	NR_PCACHE_SET_FLAGS
};

#define TEST_PSET_FLAGS(uname, lname)				\
static inline int Pset##uname(const struct pcache_set *p)	\
{								\
	return test_bit(PCACHE_SET_##lname, &p->flags);		\
}

#define SET_PSET_FLAGS(uname, lname)				\
static inline void SetPset##uname(struct pcache_set *p)		\
{								\
	set_bit(PCACHE_SET_##lname, &p->flags);			\
}

#define __SET_PSET_FLAGS(uname, lname)				\
static inline void __SetPset##uname(struct pcache_set *p)	\
{								\
	__set_bit(PCACHE_SET_##lname, &p->flags);		\
}

#define CLEAR_PSET_FLAGS(uname, lname)				\
static inline void ClearPset##uname(struct pcache_set *p)	\
{								\
	clear_bit(PCACHE_SET_##lname, &p->flags);		\
}

#define __CLEAR_PSET_FLAGS(uname, lname)			\
static inline void __ClearPset##uname(struct pcache_set *p)	\
{								\
	__clear_bit(PCACHE_SET_##lname, &p->flags);		\
}

#define PSET_FLAGS(uname, lname)				\
	TEST_PSET_FLAGS(uname, lname)				\
	SET_PSET_FLAGS(uname, lname)				\
	CLEAR_PSET_FLAGS(uname, lname)				\
	__SET_PSET_FLAGS(uname, lname)				\
	__CLEAR_PSET_FLAGS(uname, lname)

PSET_FLAGS(Evicting, evicting)
PSET_FLAGS(Sweeping, sweeping)

/*
 * struct pcache_meta bits
 */

/*
 * pcacheline->bits
 *
 * PC_locked:		Pcacheline is locked. DO NOT TOUCH.
 * 			e.g., under rmap operations
 *
 * PC_allocated:	Pcacheline is allocated, but may not be usable (internal)
 * PC_usable:		Pcacheline is usable, for all users (public)
 *
 * PC_valid:		Pcacheline has a valid mapping and content.
 * 			Depends on if there are rmap, thus set/clear by rmap functions.
 * 			Only valid pcache line can be evicted.
 *
 * PC_dirty:		Pcacheline is dirty
 * PC_reclaim:		Pcacheline was selected to be evicted
 * PC_writeback:	Pcacheline is being writtern back to memory
 * 			Only set/clear by flush routine
 *
 * Hack: remember to update the pcacheflag_names array in debug file.
 *
 * Note:
 * 1) pcache allocator use the PC_allocated bit to guard allocation.
 * Once a cache line is selected, the PC_allocated bit is set. However,
 * allocator still needs to perform some initial setup before return to
 * caller. PC_uable is set once all setuo is done, and it means this
 * cache line can be used safely by all code.
 *
 * 2) PC_valid is more like the traditional cache valid bit. It is set when
 * the pcache line has established a valid mapping to user pgtable.
 * 
 * 3) In a pcache's life time, the transition of different states is:
 *
 *        Locked  Allocated  Usable  Valid  Dirty  Writeback
 * Free:
 *        0       0          0       0      0      0
 * Alloc:
 *        0       1          0       0      0      0	(pcache_alloc_fastpath())
 *        0       1          1       0      0      0	( ..set_pcache_usable())
 *        0       1          1       1      0      0	(common_do_fill_page() after pte_set)
 *
 *        0       0          0       0      0      0
 *
 * 4) In theory, eviction algorithm should pick lines with
 * 	Allocated & Usable & Valid bits set.
 */
enum pcache_meta_bits {
	PC_locked,
	PC_allocated,
	PC_usable,
	PC_valid,
	PC_dirty,
	PC_reclaim,
	PC_writeback,

	__NR_PCLBITS,
};

#define TEST_PC_BITS(uname, lname)				\
static inline int Pcache##uname(const struct pcache_meta *p)	\
{								\
	return test_bit(PC_##lname, (void *)&p->bits);		\
}

#define SET_PC_BITS(uname, lname)				\
static inline void SetPcache##uname(struct pcache_meta *p)	\
{								\
	set_bit(PC_##lname, (void *)&p->bits);			\
}

#define CLEAR_PC_BITS(uname, lname)				\
static inline void ClearPcache##uname(struct pcache_meta *p)	\
{								\
	clear_bit(PC_##lname, (void *)&p->bits);		\
}

#define __SET_PC_BITS(uname, lname)				\
static inline void __SetPcache##uname(struct pcache_meta *p)	\
{								\
	__set_bit(PC_##lname, (void *)&p->bits);		\
}

#define __CLEAR_PC_BITS(uname, lname)				\
static inline void __ClearPcache##uname(struct pcache_meta *p)	\
{								\
	__clear_bit(PC_##lname, (void *)&p->bits);		\
}

#define TEST_SET_BITS(uname, lname)				\
static inline int TestSetPcache##uname(struct pcache_meta *p)	\
{								\
	return test_and_set_bit(PC_##lname, (void *)&p->bits);	\
}

#define TEST_CLEAR_BITS(uname, lname)				\
static inline int TestClearPcache##uname(struct pcache_meta *p)	\
{								\
	return test_and_clear_bit(PC_##lname, (void *)&p->bits);\
}

#define __TEST_SET_BITS(uname, lname)				\
static inline int __TestSetPcache##uname(struct pcache_meta *p)	\
{								\
	return __test_and_set_bit(PC_##lname, (void *)&p->bits);\
}

#define __TEST_CLEAR_BITS(uname, lname)				\
static inline int __TestClearPcache##uname(struct pcache_meta *p)\
{								\
	return __test_and_clear_bit(PC_##lname, (void *)&p->bits);\
}

#define PCACHE_META_BITS(uname, lname)				\
	TEST_PC_BITS(uname, lname)				\
	SET_PC_BITS(uname, lname)				\
	CLEAR_PC_BITS(uname, lname)				\
	__SET_PC_BITS(uname, lname)				\
	__CLEAR_PC_BITS(uname, lname)				\
	TEST_SET_BITS(uname, lname)				\
	TEST_CLEAR_BITS(uname, lname)				\
	__TEST_SET_BITS(uname, lname)				\
	__TEST_CLEAR_BITS(uname, lname)

PCACHE_META_BITS(Locked, locked)
PCACHE_META_BITS(Allocated, allocated)
PCACHE_META_BITS(Usable, usable)
PCACHE_META_BITS(Valid, valid)
PCACHE_META_BITS(Dirty, dirty)
PCACHE_META_BITS(Reclaim, reclaim)
PCACHE_META_BITS(Writeback, writeback)

/*
 * Flags checked when a pcache is freed.
 * Pcache lines being freed should not have these flags set.
 * It they are, there is a problem.
 * Basically, everything except Allocated & Usable
 */
#define PCACHE_FLAGS_CHECK_AT_FREE				\
	(1UL << PC_locked | 1UL << PC_valid | 1UL << PC_dirty |	\
	 1UL << PC_reclaim | 1UL << PC_writeback )

#endif /* _LEGO_PROCESSOR_PCACHE_TYPES_H_ */

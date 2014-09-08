/*
 * Copyright (C) 2001,2002,2003 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _COBALT_KERNEL_HEAP_H
#define _COBALT_KERNEL_HEAP_H

#include <cobalt/kernel/assert.h>
#include <cobalt/kernel/lock.h>
#include <cobalt/kernel/list.h>
#include <cobalt/kernel/trace.h>
#include <cobalt/uapi/kernel/types.h>
#include <cobalt/uapi/kernel/heap.h>

/**
 * @addtogroup cobalt_core_heap
 * @{
 *
 * @par Implementation constraints
 *
 * - Minimum page size is 2 ** XNHEAP_MINLOG2 (must be large enough to
 * hold a pointer).
 *
 * - Maximum page size is 2 ** XNHEAP_MAXLOG2.
 *
 * - Minimum block size equals the minimum page size.
 *
 * - Requested block size smaller than the minimum block size is
 * rounded to the minimum block size.
 *
 * - Requested block size larger than 2 times the page size is rounded
 * to the next page boundary and obtained from the free page list. So
 * we need a bucket for each power of two between XNHEAP_MINLOG2 and
 * XNHEAP_MAXLOG2 inclusive, plus one to honor requests ranging from
 * the maximum page size to twice this size.
 */
#define XNHEAP_PAGESZ	  PAGE_SIZE
#define XNHEAP_MINLOG2    3
#define XNHEAP_MAXLOG2    22	/* Must hold pagemap::bcount objects */
#define XNHEAP_MINALLOCSZ (1 << XNHEAP_MINLOG2)
#define XNHEAP_MINALIGNSZ (1 << 4) /* i.e. 16 bytes */
#define XNHEAP_NBUCKETS   (XNHEAP_MAXLOG2 - XNHEAP_MINLOG2 + 2)
#define XNHEAP_MAXEXTSZ   (1 << 31) /* i.e. 2Gb */

#define XNHEAP_PFREE   0
#define XNHEAP_PCONT   1
#define XNHEAP_PLIST   2

struct xnpagemap {
	unsigned int type : 8;	  /* PFREE, PCONT, PLIST or log2 */
	unsigned int bcount : 24; /* Number of active blocks. */
};

struct xnextent {
	/** xnheap->extents */
	struct list_head link;
	/** Base address of the page array */
	caddr_t membase;
	/** Memory limit of page array */
	caddr_t memlim;
	/** Head of the free page list */
	caddr_t freelist;
	/** Beginning of page map */
	struct xnpagemap pagemap[1];
};

struct xnheap {
	char name[XNOBJECT_NAME_LEN];
	unsigned long extentsize;
	unsigned long hdrsize;
	/** Number of pages per extent */
	unsigned long npages;
	unsigned long ubytes;
	unsigned long maxcont;
	struct list_head extents;
	int nrextents;

	DECLARE_XNLOCK(lock);

	struct xnbucket {
		caddr_t freelist;
		int fcount;
	} buckets[XNHEAP_NBUCKETS];

	/** heapq */
	struct list_head next;
};

extern struct xnheap kheap;

#define xnheap_extentsize(heap)		((heap)->extentsize)
#define xnheap_page_count(heap)		((heap)->npages)
#define xnheap_usable_mem(heap)		((heap)->maxcont * (heap)->nrextents)
#define xnheap_used_mem(heap)		((heap)->ubytes)
#define xnheap_max_contiguous(heap)	((heap)->maxcont)

static inline size_t xnheap_align(size_t size, size_t al)
{
	/* The alignment value must be a power of 2 */
	return ((size+al-1)&(~(al-1)));
}

static inline size_t xnheap_external_overhead(size_t hsize)
{
	size_t pages = (hsize + XNHEAP_PAGESZ - 1) / XNHEAP_PAGESZ;
	return xnheap_align(sizeof(struct xnextent)
			    + pages * sizeof(struct xnpagemap), XNHEAP_PAGESZ);
}

static inline size_t xnheap_internal_overhead(size_t hsize)
{
	/* o = (h - o) * m / p + e
	   o * p = (h - o) * m + e * p
	   o * (p + m) = h * m + e * p
	   o = (h * m + e *p) / (p + m)
	*/
	return xnheap_align((sizeof(struct xnextent) * XNHEAP_PAGESZ
			     + sizeof(struct xnpagemap) * hsize)
			    / (XNHEAP_PAGESZ + sizeof(struct xnpagemap)), XNHEAP_PAGESZ);
}

#define xnmalloc(size)     xnheap_alloc(&kheap,size)
#define xnfree(ptr)        xnheap_free(&kheap,ptr)

static inline size_t xnheap_rounded_size(size_t hsize)
{
	/*
	 * Account for the minimum heap size (i.e. 2 * page size) plus
	 * overhead so that the actual heap space is large enough to
	 * match the requested size. Using a small page size for large
	 * single-block heaps might reserve a lot of useless page map
	 * memory, but this should never get pathological anyway,
	 * since we only consume 4 bytes per page.
	 */
	if (hsize < 2 * XNHEAP_PAGESZ)
		hsize = 2 * XNHEAP_PAGESZ;
	hsize += xnheap_external_overhead(hsize);
	return xnheap_align(hsize, XNHEAP_PAGESZ);
}

/* Private interface. */

#ifdef CONFIG_XENO_OPT_VFILE
void xnheap_init_proc(void);
void xnheap_cleanup_proc(void);
#else /* !CONFIG_XENO_OPT_VFILE */
static inline void xnheap_init_proc(void) { }
static inline void xnheap_cleanup_proc(void) { }
#endif /* !CONFIG_XENO_OPT_VFILE */

#define xnheap_base_memory(heap) \
	((unsigned long)((heap)->heapbase))

/* Public interface. */

int xnheap_init(struct xnheap *heap,
		void *heapaddr,
		unsigned long heapsize);

void xnheap_set_name(struct xnheap *heap,
		     const char *name, ...);

void xnheap_destroy(struct xnheap *heap,
		    void (*flushfn)(struct xnheap *heap,
				    void *extaddr,
				    unsigned long extsize,
				    void *cookie),
		    void *cookie);

int xnheap_extend(struct xnheap *heap,
		  void *extaddr,
		  unsigned long extsize);

void *xnheap_alloc(struct xnheap *heap,
		   unsigned long size);

int xnheap_test_and_free(struct xnheap *heap,
			 void *block,
			 int (*ckfn)(void *block));

int xnheap_free(struct xnheap *heap,
		void *block);

int xnheap_check_block(struct xnheap *heap,
		       void *block);

/** @} */

#endif /* !_COBALT_KERNEL_HEAP_H */

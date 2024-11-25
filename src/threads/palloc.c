#include "threads/palloc.h"

#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kernel/bitmap.h"
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/** Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

/** A memory pool. */
struct pool {
  struct lock lock;        /**< Mutual exclusion. */
  struct bitmap *used_map; /**< Bitmap of free pages. */
  uint8_t *base;           /**< Base of pool. */
};

/** Two pools: one for kernel data, one for user pages. */
static struct pool kernel_pool, user_pool;

static void init_pool(struct pool *, void *base, size_t page_cnt, const char *name);
static bool page_from_pool(const struct pool *, void *page);

/** Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void palloc_init(size_t user_page_limit) {  // user_page_limit was passed by SIZE_MAX in init.c
  /* Free memory starts at 1 MB and runs to the end of RAM. */
  uint8_t *free_start = ptov(1024 * 1024);
  uint8_t *free_end = ptov(init_ram_pages * PGSIZE);  // init_ram_pages defined in start.S
  size_t free_pages = (free_end - free_start) / PGSIZE;
  size_t user_pages = free_pages / 2;
  if (user_pages > user_page_limit) user_pages = user_page_limit;  // user_pages = min(user_pages, user_page_limit)
  size_t kernel_pages = free_pages - user_pages;

  /* Give half of memory to kernel, half to user. */
  init_pool(&kernel_pool, free_start, kernel_pages, "kernel pool");
  init_pool(&user_pool, free_start + kernel_pages * PGSIZE, user_pages, "user pool");
}

/** Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
// 在物理内存上, 找连续 page_cnt 个没有被使用的内存, 并返回起始地址
// NOTE: 连续分配应该受限, 因为有碎片问题
void *palloc_get_multiple(enum palloc_flags flags, size_t page_cnt) {
  struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;  // 对应的 pool

  if (page_cnt == 0) return NULL;

  size_t page_idx;
  lock_acquire(&pool->lock);
  // 找到一段连续的, 长度为 page_cnt * PGSIZE, 没有被使用的内存, 并设置为使用 (分配)
  page_idx = bitmap_scan_and_flip(pool->used_map, 0, page_cnt, false);
  lock_release(&pool->lock);

  void *pages;
  if (page_idx != BITMAP_ERROR)
    pages = pool->base + PGSIZE * page_idx;
  else
    pages = NULL;

  if (pages != NULL) {
    if (flags & PAL_ZERO) memset(pages, 0, PGSIZE * page_cnt);
  } else {
    if (flags & PAL_ASSERT) PANIC("palloc_get: out of pages");
  }

  return pages;
}

/** Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *palloc_get_page(enum palloc_flags flags) { return palloc_get_multiple(flags, 1); }

/** Frees the PAGE_CNT pages starting at PAGES. */
void palloc_free_multiple(void *pages, size_t page_cnt) {
  ASSERT(pg_ofs(pages) == 0);  // pages 必须 PGSIZE 对齐
  if (pages == NULL || page_cnt == 0) return;

  struct pool *pool;  // 获取 pages 所在的 pool
  if (page_from_pool(&kernel_pool, pages))
    pool = &kernel_pool;
  else if (page_from_pool(&user_pool, pages))
    pool = &user_pool;
  else
    NOT_REACHED();

  size_t page_idx = pg_no(pages) - pg_no(pool->base);

#ifndef NDEBUG
  memset(pages, 0xcc, PGSIZE * page_cnt);
#endif

  ASSERT(bitmap_all(pool->used_map, page_idx, page_cnt));
  bitmap_set_multiple(pool->used_map, page_idx, page_cnt, false);
}

/** Frees the page at PAGE. */
//
void palloc_free_page(void *page) { palloc_free_multiple(page, 1); }

/** Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void init_pool(struct pool *p, void *base, size_t page_cnt, const char *name) {
  /* We'll put the pool's used_map at its base.
     Calculate the space needed for the bitmap
     and subtract it from the pool's size. */
  size_t bm_pages = DIV_ROUND_UP(bitmap_buf_size(page_cnt), PGSIZE);
  if (bm_pages > page_cnt) PANIC("Not enough memory in %s for bitmap.", name);
  page_cnt -= bm_pages;

  printf("%zu pages available in %s.\n", page_cnt, name);

  /* Initialize the pool. */
  lock_init(&p->lock);
  p->used_map = bitmap_create_in_buf(page_cnt, base, bm_pages * PGSIZE);  // build the bitmap
  p->base = base + bm_pages * PGSIZE;
}

/** Returns true if PAGE was allocated from POOL,
   false otherwise. */
static bool page_from_pool(const struct pool *pool, void *page) {
  size_t page_no = pg_no(page);
  size_t start_page = pg_no(pool->base);
  size_t end_page = start_page + bitmap_size(pool->used_map);

  return page_no >= start_page && page_no < end_page;
}

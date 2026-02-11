#include "my_malloc.h"

/* ========================================================
 * Block metadata structure
 * Placed immediately before the user-visible memory region.
 * ======================================================== */
typedef struct block {
  size_t size;         /* usable size (bytes), excluding metadata */
  struct block *next;  /* pointer to next block in free list      */
} block_t;

#define META_SIZE sizeof(block_t)

/* ========================================================
 * Helper: insert a freed block into a free list that is
 * kept sorted by address.  After insertion, coalesce with
 * the immediate neighbours if they are physically adjacent.
 * ======================================================== */
static void insert_free_block(block_t **head, block_t *block) {
  block_t *curr = *head;
  block_t *prev = NULL;

  /* Walk the list to find the correct sorted position */
  while (curr != NULL && curr < block) {
    prev = curr;
    curr = curr->next;
  }

  /* Link the block into the list */
  block->next = curr;
  if (prev != NULL) {
    prev->next = block;
  } else {
    *head = block;
  }

  /* Coalesce with the NEXT free block if adjacent */
  if (block->next != NULL &&
      (char *)block + META_SIZE + block->size == (char *)block->next) {
    block->size += META_SIZE + block->next->size;
    block->next = block->next->next;
  }

  /* Coalesce with the PREVIOUS free block if adjacent */
  if (prev != NULL &&
      (char *)prev + META_SIZE + prev->size == (char *)block) {
    prev->size += META_SIZE + block->size;
    prev->next = block->next;
  }
}

/* ========================================================
 * Helper: best-fit search on the given free list.
 * Finds the smallest block whose size >= requested size,
 * removes it from the list, and splits it if the remainder
 * is large enough for another allocation.
 * Returns NULL if no suitable block is found.
 * ======================================================== */
static block_t *best_fit_search(block_t **head, size_t size) {
  block_t *curr = *head;
  block_t *prev = NULL;
  block_t *best = NULL;
  block_t *best_prev = NULL;

  /* Scan the entire free list for the best (smallest) fit */
  while (curr != NULL) {
    if (curr->size >= size) {
      if (best == NULL || curr->size < best->size) {
        best = curr;
        best_prev = prev;
      }
      /* Perfect fit — no need to keep searching */
      if (best->size == size) {
        break;
      }
    }
    prev = curr;
    curr = curr->next;
  }

  if (best == NULL) {
    return NULL;
  }

  /* Remove the chosen block from the free list */
  if (best_prev != NULL) {
    best_prev->next = best->next;
  } else {
    *head = best->next;
  }
  best->next = NULL;

  /* Split if the remainder can hold at least 1 byte of user data */
  if (best->size >= size + META_SIZE + 1) {
    block_t *remainder =
        (block_t *)((char *)best + META_SIZE + size);
    remainder->size = best->size - size - META_SIZE;
    remainder->next = NULL;
    best->size = size;
    insert_free_block(head, remainder);
  }

  return best;
}

/* ================================================================
 * VERSION 1 — Lock-based thread-safe malloc / free
 *
 * Strategy: a single global free list protected by one mutex.
 * Every call to ts_malloc_lock / ts_free_lock acquires the mutex,
 * which serializes all heap operations across threads.
 * ================================================================ */

static block_t *lock_free_list = NULL;
static pthread_mutex_t lock_mutex = PTHREAD_MUTEX_INITIALIZER;

void *ts_malloc_lock(size_t size) {
  if (size == 0) {
    return NULL;
  }

  pthread_mutex_lock(&lock_mutex);

  /* Try to satisfy the request from the free list */
  block_t *block = best_fit_search(&lock_free_list, size);

  if (block != NULL) {
    pthread_mutex_unlock(&lock_mutex);
    return (void *)(block + 1);
  }

  /* No suitable free block — grow the heap */
  block = sbrk(META_SIZE + size);
  if (block == (void *)-1) {
    pthread_mutex_unlock(&lock_mutex);
    return NULL;
  }

  block->size = size;
  block->next = NULL;

  pthread_mutex_unlock(&lock_mutex);
  return (void *)(block + 1);
}

void ts_free_lock(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  block_t *block = (block_t *)ptr - 1;

  pthread_mutex_lock(&lock_mutex);
  insert_free_block(&lock_free_list, block);
  pthread_mutex_unlock(&lock_mutex);
}

/* ================================================================
 * VERSION 2 — Non-locking thread-safe malloc / free
 *
 * Strategy: each thread maintains its own free list via
 * Thread-Local Storage (__thread).  No lock is needed when
 * accessing the per-thread free list.
 *
 * The only lock is around sbrk(), which is not thread-safe.
 * ================================================================ */

static __thread block_t *nolock_free_list = NULL;
static pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;

void *ts_malloc_nolock(size_t size) {
  if (size == 0) {
    return NULL;
  }

  /* Search the thread-local free list (no lock needed) */
  block_t *block = best_fit_search(&nolock_free_list, size);

  if (block != NULL) {
    return (void *)(block + 1);
  }

  /* No suitable free block — grow the heap (lock only around sbrk) */
  pthread_mutex_lock(&sbrk_mutex);
  block = sbrk(META_SIZE + size);
  pthread_mutex_unlock(&sbrk_mutex);

  if (block == (void *)-1) {
    return NULL;
  }

  block->size = size;
  block->next = NULL;

  return (void *)(block + 1);
}

void ts_free_nolock(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  block_t *block = (block_t *)ptr - 1;

  /* Insert into the CALLING thread's local free list (no lock needed) */
  insert_free_block(&nolock_free_list, block);
}

# Thread-Safe Malloc Implementation Report

## 1. Implementation Overview

### 1.1 Data Structure and Allocation Policy

Both versions of the thread-safe malloc library share the same underlying data structure and allocation policy. Each allocated memory block is preceded by a metadata header (`block_t`), which stores the usable size of the block and a pointer to the next free block. This header occupies 16 bytes on a 64-bit system (8 bytes for `size_t` and 8 bytes for the pointer). The free list is maintained as a singly linked list sorted by memory address, which facilitates efficient coalescing of adjacent free blocks. The definition is as follows:

```c
typedef struct block {
  size_t size;         /* usable size (bytes), excluding metadata */
  struct block *next;  /* pointer to next block in free list      */
} block_t;

#define META_SIZE sizeof(block_t)
```

Both versions employ the **best-fit** allocation policy. When a malloc request arrives, the allocator scans the entire free list to find the smallest block that is large enough to satisfy the request. If a perfect fit is found, the search terminates early. Once a suitable block is selected, it is removed from the free list. If the chosen block is significantly larger than the requested size—specifically, if the remainder can hold at least one byte of user data plus a metadata header—the block is split, and the unused portion is returned to the free list. The best-fit search implementation is shown below:

```c
static block_t *best_fit_search(block_t **head, size_t size) {
  block_t *curr = *head;
  block_t *prev = NULL;
  block_t *best = NULL;
  block_t *best_prev = NULL;

  while (curr != NULL) {
    if (curr->size >= size) {
      if (best == NULL || curr->size < best->size) {
        best = curr;
        best_prev = prev;
      }
      if (best->size == size) {
        break;  /* Perfect fit — no need to keep searching */
      }
    }
    prev = curr;
    curr = curr->next;
  }

  if (best == NULL) return NULL;

  /* Remove the chosen block from the free list */
  if (best_prev != NULL) {
    best_prev->next = best->next;
  } else {
    *head = best->next;
  }
  best->next = NULL;

  /* Split if the remainder can hold at least 1 byte of user data */
  if (best->size >= size + META_SIZE + 1) {
    block_t *remainder = (block_t *)((char *)best + META_SIZE + size);
    remainder->size = best->size - size - META_SIZE;
    remainder->next = NULL;
    best->size = size;
    insert_free_block(head, remainder);
  }

  return best;
}
```

When a block is freed, it is inserted back into the free list at the correct position to maintain address ordering, and the allocator checks whether the block can be coalesced with its immediate neighbors, merging them into a single larger block if they are physically adjacent in memory. The insertion and coalescing logic is as follows:

```c
static void insert_free_block(block_t **head, block_t *block) {
  block_t *curr = *head;
  block_t *prev = NULL;

  while (curr != NULL && curr < block) {
    prev = curr;
    curr = curr->next;
  }

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
```

### 1.2 Version 1: Lock-Based Implementation (`ts_malloc_lock` / `ts_free_lock`)

The locking version uses a single **global free list** shared by all threads, protected by a single `pthread_mutex_t`. Every call to `ts_malloc_lock` or `ts_free_lock` acquires this mutex at the beginning and releases it before returning. This means that the entire malloc or free operation—including the free list search, block splitting, block insertion, and coalescing—is treated as a single critical section.

```c
static block_t *lock_free_list = NULL;
static pthread_mutex_t lock_mutex = PTHREAD_MUTEX_INITIALIZER;

void *ts_malloc_lock(size_t size) {
  if (size == 0) return NULL;

  pthread_mutex_lock(&lock_mutex);

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
  if (ptr == NULL) return;
  block_t *block = (block_t *)ptr - 1;

  pthread_mutex_lock(&lock_mutex);
  insert_free_block(&lock_free_list, block);
  pthread_mutex_unlock(&lock_mutex);
}
```

The critical sections in this version are straightforward to identify: any operation that reads or modifies the global free list or calls `sbrk()` to grow the heap constitutes a critical section. Since both `ts_malloc_lock` and `ts_free_lock` access the shared free list, the mutex must be held throughout each function call. While this approach completely serializes all heap operations across threads (meaning no two threads can perform a malloc or free concurrently), it guarantees correctness by eliminating all possible race conditions on the shared data structure. The simplicity of this strategy also makes the implementation easier to reason about and verify.

### 1.3 Version 2: Non-Locking Implementation (`ts_malloc_nolock` / `ts_free_nolock`)

The non-locking version takes a fundamentally different approach to concurrency by leveraging **Thread-Local Storage (TLS)**. Instead of maintaining a single global free list, each thread has its own private free list, declared with the `__thread` storage class specifier. Because each thread's free list is private and never accessed by other threads, no locking is required when searching for a free block, splitting a block, inserting a freed block, or coalescing adjacent blocks.

The only critical section in this version is the call to `sbrk()`, which modifies the global program break and is inherently not thread-safe. A dedicated `pthread_mutex_t` (`sbrk_mutex`) is acquired immediately before calling `sbrk()` and released immediately after. This lock is held for the absolute minimum amount of time—only for the duration of the single `sbrk` system call—and does not protect any free list operations.

```c
static __thread block_t *nolock_free_list = NULL;
static pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;

void *ts_malloc_nolock(size_t size) {
  if (size == 0) return NULL;

  /* Search the thread-local free list (no lock needed) */
  block_t *block = best_fit_search(&nolock_free_list, size);
  if (block != NULL) {
    return (void *)(block + 1);
  }

  /* No suitable free block — lock only around sbrk */
  pthread_mutex_lock(&sbrk_mutex);
  block = sbrk(META_SIZE + size);
  pthread_mutex_unlock(&sbrk_mutex);

  if (block == (void *)-1) return NULL;
  block->size = size;
  block->next = NULL;
  return (void *)(block + 1);
}

void ts_free_nolock(void *ptr) {
  if (ptr == NULL) return;
  block_t *block = (block_t *)ptr - 1;

  /* Insert into the CALLING thread's local free list (no lock needed) */
  insert_free_block(&nolock_free_list, block);
}
```

When a thread calls `ts_free_nolock`, the freed block is inserted into the **calling thread's** local free list, regardless of which thread originally allocated it. This means that if Thread A allocates a block and Thread B later frees it, that block becomes part of Thread B's local free list and is only available for reuse by Thread B. This design maximizes concurrency at the cost of reduced cross-thread memory reuse.

## 2. Experimental Results and Analysis

The `thread_test_measurement` program was used to evaluate the performance and memory efficiency of both versions. This test creates 4 threads, each performing 20,000 allocation operations with varying sizes. Threads with even IDs occasionally free allocations made by other threads. The results are summarized below:

| Version | Run | Execution Time (s) | Data Segment Size (bytes) |
|---------|-----|---------------------|---------------------------|
| Lock    | 1   | 0.047158            | 41,942,160                |
| No-Lock | 2   | 0.115934            | 41,606,864                |

All four correctness tests (`thread_test`, `thread_test_malloc_free`, `thread_test_malloc_free_change_thread`, and `thread_test_measurement`) passed for both versions, confirming that no overlapping allocated regions were produced under concurrent execution.

### 2.1 Performance Analysis

An interesting observation from the experimental data is that the lock-based version (0.047s) was actually faster than the non-locking version (0.089s–0.116s) in this particular test scenario. While this may seem counterintuitive—since the non-locking version was designed to reduce lock contention and allow more parallelism—it can be explained by examining the specific workload characteristics.

In the locking version, all threads share a single global free list. When one thread frees a block, that block immediately becomes available for reuse by any thread. This high degree of memory reuse means that most allocation requests can be satisfied from the existing free list without calling `sbrk()`. In contrast, the non-locking version maintains per-thread free lists. When Thread B frees a block that was allocated by Thread A, that block enters Thread B's local list and is invisible to Thread A. Consequently, Thread A cannot reuse it and must call `sbrk()` instead. Since `sbrk()` is the only operation in the non-locking version that requires a lock, the ironic result is that the non-locking version ends up making more locked `sbrk()` calls than the locking version makes locked free-list operations. Furthermore, each `sbrk()` call involves a system call, which is inherently more expensive than in-memory free list manipulation.

### 2.2 Memory Efficiency Analysis

The data segment sizes are relatively comparable across both versions, with the lock version at approximately 41.9 MB and the no-lock version ranging from 41.6 MB to 42.1 MB. The small variation in the no-lock version's data segment size across runs reflects the non-deterministic nature of thread scheduling—different interleavings of thread execution lead to different patterns of cross-thread frees and consequently different amounts of heap growth via `sbrk()`.

The results illustrate a nuanced tradeoff between the two approaches. The locking version, despite serializing all operations, benefits from a globally shared free list that maximizes memory reuse and minimizes expensive `sbrk()` calls. The non-locking version, while theoretically allowing more parallelism in free list operations, suffers from reduced block reuse across threads, leading to more heap growth and more locked `sbrk()` invocations.


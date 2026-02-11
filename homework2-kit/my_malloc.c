#include "my_malloc.h"

typedef struct block{
    size_t size;
    struct block * next; // the type of this pointer is struct block
} block_t;

#define META_SIZE sizeof(block_t)

static void insert_free_block(block_t **head, block_t * block){
    //? Why static
    // block_t** head is the pointer which points to head pointer of the list
    // block_t * block is the new block, waiting to be inserted

    block_t *curr = *head;
    block_t *prev = NULL;

    while(curr!= NULL && curr < block){
        // pointer essentially is the value of an address.
        // so curr < block means the address of current block is smaller than new block's address
        prev = curr;
        curr = curr->next;
    }

    block->next = curr;
    if(prev != NULL){
        prev->next = block;
    }else{
        *head = block;
    }

    //? Why we need to convert to char to compare
    if(block->next != NULL &&
    (char *)block + META_SIZE + block->size == (char*)block->next){
        block->size += META_SIZE  + block->next->size;
        block->next = block->next->next;
    }

    if(prev != NULL &&
    (char*) prev + META_SIZE + prev->size == (char*) block){
        prev->size += META_SIZE + block->size;
        prev->next = block->next;
    }
}

static block_t *best_fit_search(block_t** head, size_t size){
    block_t *curr = *head;
    block_t *prev = NULL;
    block_t *best = NULL;
    block_t *best_prev = NULL;

    while(curr != NULL){
        if(curr->size >= size){
            if(best == NULL || curr->size < best->size){
                best = curr;
                best_prev = prev;
            }

            if(best->size == size){
                break;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    if(best == NULL){
        return NULL;
    }

    if(best_prev != NULL){
        best_prev->next = best->next;
    }else{
        *head = best->next;
    }
    best->next = NULL;

    if(best->size >= size + META_SIZE + 1){
        //This line find the address of the remainded(extra) part of block
        block_t * remainder = 
        (block_t *)((char*)best + META_SIZE + size);

        remainder->size = best->size - size - META_SIZE;
        remainder->next = NULL;
        best->size = size;
        insert_free_block(head, remainder);
    }

    return best;
}

//? why static
static block_t * lock_free_list = NULL;
static pthread_mutex_t lock_mutex = PTHREAD_MUTEX_INITIALIZER;
// ? what is PTHREAD_MUTEX_INITIALIZER

void *ts_malloc_lock(size_t size){
    if(size == 0){
        return NULL;
    }

    //? I am not familiar with programming on threads
    pthread_mutex_lock(&lock_mutex);

    block_t * block = best_fit_search(&lock_free_list, size);

    if(block != NULL){
        pthread_mutex_unlock(&lock_mutex);
        return (void*)(block+1);
    }

    block = sbrk(META_SIZE + size);

    // ? what does -1 mean
    if(block == (void*)-1){
        pthread_mutex_unlock(&lock_mutex);
        return NULL;
    }

    block->size = size;
    block->next = NULL;

    pthread_mutex_unlock(&lock_mutex);

    return (void*)(block + 1);
}

void ts_free_lock(void* ptr){
    if(ptr == NULL){
        return;
    }

    block_t * block = (block_t *) ptr - 1;
    pthread_mutex_lock(&lock_mutex);
    insert_free_block(&lock_free_list, block);
    pthread_mutex_unlock(&lock_mutex);
}

// ? what is 本地线程库 __thread
static __thread block_t * nolock_free_list = NULL;
static pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;
// ? what is PTHREAD_MUTEX_INITIALIZER

void *ts_malloc_nolock(size_t size){
    if(size == 0){
        return NULL;
    }


    block_t * block = best_fit_search(&nolock_free_list, size);

    if(block != NULL){
        return (void*)(block+1);
    }

    pthread_mutex_lock(&sbrk_mutex);
    block = sbrk(META_SIZE + size);
    pthread_mutex_unlock(&sbrk_mutex);

    // ? what does -1 mean
    if(block == (void*)-1){
        return NULL;
    }

    block->size = size;
    block->next = NULL;

    return (void*)(block + 1);
}

void ts_free_lock(void* ptr){
    if(ptr == NULL){
        return;
    }

    block_t * block = (block_t *) ptr - 1;
    insert_free_block(&nolock_free_list, block);
}
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
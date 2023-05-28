/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char *blk_addr_arr_start; /* Start Addr of arr for free block list*/
static char *blk_addr_arr_end; /* End Addr of arr for free block list*/

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *heap_start;
    char *heap_end;
    mem_init();  /* Max Heap initialized */

    if (mem_sbrk(16*WSIZE) == (void *)-1)     /* Create initial empty heap, prologue footer/header(2) + epilogue(1) + arr(12) + padding(1) =  16 words */
        return -1;
    heap_start = mem_heap_lo();
    heap_end = mem_heap_hi();
    PUT(heap_start, 0);   /* Alignment padding */
    free_blk_arr_init();  /* Index arr of free block list head addr */
    blk_addr_arr_start = heap_start + (WSIZE);

    PUT(heap_start + ((ARR_SIZE + 1) * WSIZE), PACK(DSIZE, 1));  /* Prologue header */
    PUT(heap_start + ((ARR_SIZE + 2) * WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
    
    PUT(heap_start + ((ARR_SIZE + 3) * WSIZE), PACK(DSIZE, 1));  /* Epilogue header */

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * free block list set as fellow :
 * {16, 31}, {32, 63} …..  {2^15 ~ +∞}   2^4 → 2^15
 *  class 1  ,  class 2     class 12
*/
void free_blk_arr_init (void) 
{
    int i;
    size_t avail_size;
    char *heap_start = mem_heap_lo();

    for (i = 1; i < ARR_SIZE + 1; i++) {
        PUT(heap_start + (i * WSIZE), 0);
    }

}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;  /* Adjusted block size */
    size_t extend_size; 
    char *bp;

    if (size == 0)
        return NULL;
    size = ALIGN(size);  /* Requirement: allocated size aligned to 8 */
    
    asize = DSIZE + DSIZE + size; /* Block Size : Header/Footer + Pred/Succ + payload */ 

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No Avail space */
    extend_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size/WSIZE)) != NULL) {
        place(bp, asize);
        return bp;
    }
    else {
        return NULL;
    }

    
}

/*
 * mm_free - Freeing a block.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/* Given a block size, return corresponding free block list ** arr addr ** */
void *size2addr(size_t size)
{
    size_t blk_size = 1 << 4;
    size_t max_size = 1 << (4 + ARR_SIZE);  /* Last block list ranges from 4096 to ∞ */
    size_t arr_idx = 0;
    if (size < blk_size)
        return NULL;
    while (arr_idx < ARR_SIZE) {
        if (size >= blk_size && size <= (blk_size << 1)) {
            break;
        }
        blk_size = blk_size << 1;
        arr_idx ++;
    }

    return (void*) (blk_addr_arr_start + (arr_idx*WSIZE));
}

/*
 * Delete a free block from list
 * Split the block if needed
*/
void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    delelete_node(bp);
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        add_free_blk(bp);
    }
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
}

/*
 * Return addr of free block according to asize
  *  First fit algorithm
 *   If no propriate block matched, return NULL
*/
void *find_fit(size_t asize)
{
    char *arr_addr;
    char *list_addr;
    char *free_blk = NULL;
    size_t offset = 0;

    for (arr_addr = size2addr(asize); arr_addr < (blk_addr_arr_start + (ARR_SIZE*WSIZE)); arr_addr += WSIZE) {
        list_addr = (char *)GET(arr_addr);
        if (list_addr != NULL) {
            free_blk = find_node(list_addr, asize);
            break;
        }
    }
    return free_blk;

}

void *find_node(char *list_addr, size_t size)
{
    size_t blk_size;
    if (list_addr == NULL)
        return NULL;
    while(list_addr != NULL) {
        blk_size = GET_SIZE(HDRP(list_addr));
        if (blk_size >= size)
            break;
        list_addr = (char *)GET_SUCC(list_addr);
    }
    return (void *)list_addr;
}

/*
 * Add a free block to its list head: 
 *    case 1: list contains no node
 *    case 2: normal list add
 * return -1 if block isn't free
 * return 0 when done successfully
*/
int add_free_blk(char *bp)
{   
    char *arr_addr, *list_addr;
    size_t size = GET_SIZE(HDRP(bp));
    int alloc = GET_ALLOC(HDRP(bp));
    if (alloc == 1)
        return -1;
    arr_addr = size2addr(size);
    list_addr = (char *)GET(arr_addr);
    if (list_addr == NULL) {   /* List stores no free block*/
        PUT(PREDP(bp), 0);  /* Set prev node to NULL */
        PUT(SUCCP(bp), 0);  /* Set succ node to NULL */
        PUT(arr_addr, bp);  /* Update addr arr of free blk list */
    }
    else {
        PUT(PREDP(list_addr), bp); /* Set prev of current head to new node */
        PUT(PREDP(bp), 0);  /* Set prev node of new node to NULL */
        PUT(SUCCP(bp), list_addr); /* Set next node tof new node to current head */
        PUT(arr_addr, bp);  /* Update addr arr of free blk list */
    }
    return 0;
    
}
/* Basic List Operation */

/*
 * delete a node in free block list
 *  case 1: normal delete
 *  case 2: the only node deleted
 *  case 3: node not exited
 */
int delelete_node(void *bp)
{
    if (bp == NULL)
        return -1;
    size_t size = GET_SIZE(HDRP(bp));
    char *arr_addr = size2addr(size);
    char *prev_node = GET_PRED(bp);
    char *next_node = GET_SUCC(bp);

    if (prev_node == NULL && next_node == NULL) {
        PUT(arr_addr, 0);
        return 0;
    }
    if (prev_node != NULL)
        PUT(SUCCP(prev_node), next_node);
    if (next_node != NULL)
        PUT(PREDP(next_node), prev_node);

    return 0;
}

/*
 *  extend heap as given words
 */
 void *extend_heap(size_t words)
 {
    char *bp;
    size_t size;

    words = words < 4 ? 4: words; /* Min size for one block will be 16Byte(4 words) */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; /* Even number */

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));    /* Delete old epilgogue and set new header */
    PUT(FTRP(bp), PACK(size, 0));    /* Set footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    bp = coalesce(bp);
    return bp;

 }

 /*
  * coalesce neibor blocks
  * Before coalescing, block should not be added to free blk list 
  * Four cases are listed: 
  *   1. prev & next both not free
  *   2. prev free & next not
  *   3. prev not & next free
  *   4. prev & next both free
  * After, block will be added to free blk list
  *  
  */
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)   /* Case 1 */{
        add_free_blk(bp);
        return bp;
    }
    else if (prev_alloc == 0 && next_alloc == 1) { /* Case 2 */
        delelete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        add_free_blk(bp);
    }
    else if (prev_alloc == 1 && next_alloc == 0) { /* Case 3 */
        delelete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        add_free_blk(bp);
    }
    else {
        delelete_node(PREV_BLKP(bp));
        delelete_node(NEXT_BLKP(bp));
        size +=  (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        add_free_blk(bp);

    }

    return bp;
}















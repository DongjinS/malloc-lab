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
    "jungle3rd_week6_team4_implicit",
    /* First member's full name */
    "Dongjin Shin",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
// size < 0 -> 0, size <= 8 -> 8, size > 8 -> 8의 배수로
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

//sizeof(size_t) = 8, ALIGN(sizeof(size_t)) = 8
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4             /* word and header, footer size */
#define DSIZE 8             /* double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extended heap by this amount */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at adress p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
//p가 할당 되있는지(1) 아닌지 (0)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE)))

static void *heap_listp;
static void *prev_fit;; /* for next-fit */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *find_next_fit(size_t asize);
static void *find_best_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    /* PACK(DSIZE, 1) = 8 | 1 = 9 = 1001 = 100 / 1 = 8/1  */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);                     //실제로 사용할 위치는 epilougue header 전이기 때문에 prologue eplilogue를 가르킨다?
    prev_fit = heap_listp;
    /* Extended the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int asize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(asize);
    // if (p == (void *)-1)
    // return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    // size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    // /* Ignore spurious requests */
    // if (size == 0)
    // {
    //     return NULL;
    // }

    // /* Adjust block size to include overhead and alignment reqs. */
    // if (size <= DSIZE)
    // {
    //     asize = 2 * DSIZE;
    // }
    // else
    // {
    //     asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    // }

    /* Search the free list for a fit */
    if ((bp = find_next_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
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
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }
    // 처음(mm_init 에서 올 때) bp의 정확한 위치는 ??
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    //전 블록 가용한지
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    //다음 블록 가용한지
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    //할당 되어있으면 1
    if (prev_alloc && next_alloc)
    { /* Case 1 */
        return bp;
    }
    // 다음 블록만 할당x 가용가능 하면
    else if (prev_alloc && !next_alloc)
    { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        // 왜 이게 아니지?? - PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // 왜냐면!-- size가 이미 바껴서 FTRP(bp)하면 합쳐진 블록의 끝을 잘 찾는다!!
    }
    // 이전블록만 가가용 가능 하면
    else if (!prev_alloc && next_alloc)
    { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    prev_fit = bp;
    return bp;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    // 필요한 블록 이외에 남는게 16바이트 이상이면 - free header, footer 들어갈 자리 2워드 + payload 2워드?
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

//score: 53
static void *find_fit(size_t asize)
{
    /* First-fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL; /* no fit */

// #endif
}

//score: 82
static void *find_next_fit(size_t asize)
{
    /* Next-fit search */
    void *bp;
    //뒤에서 한번 찾고
    for (bp = prev_fit; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            prev_fit = bp;
            return bp;
        }
    }

    // 없으면 다 앞에서 부터 찾기
    for (bp = heap_listp; bp < prev_fit; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            prev_fit = bp;
            return bp;
        }
    }
    return NULL; /* no fit */
}

//score: 54
static void *find_best_fit(size_t asize)
{
    /* Best-fit search */
    void *bp;
    void *best_bp;
    int flag = 0;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if ((flag==0)&& (!GET_ALLOC(HDRP(bp))) && (asize <= GET_SIZE(HDRP(bp)))){
            flag = 1;
            best_bp = bp;
        }
        else if ((flag==1)&&(!GET_ALLOC(HDRP(bp))) && (asize <= GET_SIZE(HDRP(bp))) && (GET_SIZE(HDRP(best_bp))>GET_SIZE(HDRP(bp))))
        {   
            best_bp = bp;
        }
    }
    if (flag == 1){
        return best_bp;
    }
    else{
        return NULL; /* no fit */
    }
}


void main()
{
    void *vp;
    if (vp == NULL){
        printf("ok\n\n");
    }

    mem_init();
    mm_init();
    printf("%p\n", heap_listp);
    printf("header heap: %p \t\t header heap get data:%d \t footer heap: %p\n", HDRP(heap_listp), GET(HDRP(heap_listp)), (FTRP(heap_listp)));

    printf("header heap+2: %p \t\t header heap+1 get data:%d\n", HDRP(heap_listp+8), GET(HDRP(heap_listp+8))); 
    
    
    return;
}
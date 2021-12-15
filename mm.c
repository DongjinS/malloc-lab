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
    "jungle3rd_week6_team4_segregation",
    /* First member's full name */
    "Dongjin Shin",
    /* First member's email address */
    "gmail",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4             /* word and header, footer size */
#define DSIZE 8             /* double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extended heap by this amount */
#define LISTLIMIT 20        /* free list max num */

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
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* get free list's SUCC and PREC pointer */
#define PRED_FREE(bp) (*(char **)(bp))
#define SUCC_FREE(bp) (*(char **)(bp + WSIZE))

//static variables
static void *heap_listp;                  /* heap_list pointer */
static void *segregation_list[LISTLIMIT]; /* segregation_list pointer */

//static functions
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *place(void *bp, size_t asize);
static void remove_block(void *bp);
static void insert_block(void *bp, size_t size);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* segregation_list 초기화 - NULL 값으로 */
    int list;
    for (list = 0; list < LISTLIMIT; list++)
    {
        segregation_list[list] = NULL;
    }

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp = heap_listp + 2 * WSIZE;           /* heap_listp point to prologue footer */

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
    //할당 요청받은 size align에 맞게 사이즈 조정
    int asize = ALIGN(size + SIZE_T_SIZE);
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        bp = place(bp, asize); /* bp가 가르키는 위치에 asize만큼 할당 */
        return bp;             /* 할당된 포인터 반환 */
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    bp = place(bp, asize); /* bp가 가르키는 위치에 asize만큼 할당 */

    return bp; /* 할당된 포인터 반환 */
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr)); /* ptr 이 가르키는 블록의 사이즈를 GET */

    PUT(HDRP(ptr), PACK(size, 0)); /* 반환된 블록의 헤더 비할당으로 표시 */
    PUT(FTRP(ptr), PACK(size, 0)); /* 반환된 블록의 푸터 비할당으로 표시 */

    coalesce(ptr); /* 반환된 블록 병합 가능한지 확인 */
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size); /* 재할당 요청받은 사이즈 만큼 할당해서 newptr에 주소 저장 */
    if (newptr == NULL)       /* newptr 이 NULL이면 NULL 리턴 */
        return NULL;
    copySize = GET_SIZE(HDRP(oldptr)); /* 재할당 요청 받은 포인턴가 가르키는 블록의 사이즈 GET 해서 copysize에 저장 */

    if (size == 0)
    { /* 재할당 요청받은 사이즈가 0이면 재할당 요청 받은 블록 가용 리스트로 반환*/
        mm_free(ptr);
        return NULL;
    }

    if (size < copySize) /* 재할당 요청받은 크기가 원래 갖고 있던 사이즈 보다 작으면 copysize의 크기를 요청받은 크기로 바꾼다 */
        copySize = size;
    memcpy(newptr, oldptr, copySize); /* 원래 메모리 위치에 있던 값 새로 할당 받은 메모리 위치에 복사 */
    mm_free(oldptr);                  /* 재할당 요청받은 블록 가용리스트로 반환 */
    return newptr;                    /* 새로 할당 받은 블록 포인터 반환 */
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

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); /* 이전, 이후 블록 가용 블록인지 확인 후 값 저장 */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp)); /* 병합요청 받은 블록의 사이즈 GET - size에 저장 */

    /* 이전 혹은 이후 가용한 블록 있으면 가용리스트에서 먼저 삭제하고 병합 후 재삽입한다. */
    if (prev_alloc && next_alloc)
    { /* Case 1 이전 이후 모두 할당 */
        insert_block(bp, size);     /* 병합 요청 받은 블록 그대로 가용 리스트 삽입 */
        return bp;                  /* 가용 블록 포인터 반환 */
    }
    else if (prev_alloc && !next_alloc)
    { /* Case 2 이후 블록만 가용 */
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    { /* Case 3 이전 블록만 가용 */
        remove_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else if (!prev_alloc && !next_alloc)
    { /* Case 4 이전 이후 블록 모두 가용 */
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_block(bp, size);  /* 병합한 블록 가용 리스트 삽입 */
    return bp;               /* 가용 블록 포인터 반환 */
}

static void *place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));      /* 할당 요청 받은 가용 블록의 크기 GET */
    remove_block(bp);                       /* 할당 요청 받은 가용 블록 삭제 */
    if ((csize - asize) >= (2 * DSIZE))     /* 실제 할당할 사이즈와 할당된 블록의 사이즈 차이가 4워드 이상이면 분할 */
    {
        if (asize >= 100)                   /* 실제 할당할 사이즈기 100bytes 이상이면 가용 블록을 할당 블록 앞으로 분할 */
        {
            PUT(HDRP(bp), PACK(csize - asize, 0));
            PUT(FTRP(bp), PACK(csize - asize, 0));
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            coalesce(PREV_BLKP(bp));        /* 가용 블록 병합 요청 */
            return bp;                      /* 할당 블록 포인터 반환 */
        }                                   /* 실제 할당할 사이즈기 100bytes 미만이면 할당 블록을 가용 블록 앞으로 분할 */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp);                       /* 가용 블록 병합 요청 */
        return PREV_BLKP(bp);               /* 할당 블록 포인터 반환 */
    }   
    else                                    /* 실제 할당할 사이즈와 할당된 블록의 사이즈 차이가 4워드 미만 - 분할 X */
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        return bp;                          /* 그대로 할당하고 할당 블록 포인터 반환 */
    }
}

static void *find_fit(size_t asize)
{
    void *bp;
    int list = 0;
    size_t searchsize = asize;

    while (list < LISTLIMIT)            /* segregation list 순회 */
    {
        // 리스트 끝이거나 search 사이즈가 들어갈 segregation_list [index = list] 의 값이 NULL이 아니면 가용 리스트 탐색 시작
        if ((list == LISTLIMIT - 1) || (searchsize <= 1) && (segregation_list[list] != NULL))
        {
            bp = segregation_list[list]; /* 가용리스트 시작점 */

            while ((bp != NULL) && (asize > GET_SIZE(HDRP(bp))))  /* 요청 받은 사이즈보다 같거나 큰 사이즈의 블록을 찾거나 NULL이 나올때 까지 탐색 */
            {
                bp = SUCC_FREE(bp);
            }
            if (bp != NULL) /* 탐색을 끝냈는데 bp가 NULL이 아니면 bp 반환 */
            {
                return bp;
            }
        }
        searchsize >>= 1;   /* searchsize 우 쉬프트 연산 */
        list++;             /* segregation list index ++ */
    }

    return NULL; /* no fit */
}

static void remove_block(void *bp)
{
    int list = 0;
    size_t size = GET_SIZE(HDRP(bp)); /* 삭제해야할 블록의 사이즈 GET */

    while ((list < LISTLIMIT - 1) && (size > 1)) /* 삭제해야할 블록의 segregation list 위치 탐색 - list의 첫번째 블록을 삭제해야할 경우를 위해 */
    {
        size >>= 1;
        list++;
    }

    if (SUCC_FREE(bp) != NULL)      /* 삭제 블록의 다음 블록이 있으면 */
    {
        if (PRED_FREE(bp) != NULL) /* 삭제 블록의 이전 블록이 있으면 */
        {
            PRED_FREE(SUCC_FREE(bp)) = PRED_FREE(bp);
            SUCC_FREE(PRED_FREE(bp)) = SUCC_FREE(bp);
        }
        else                        /* 삭제 블록의 이전 블록이 없으면 = 리스트에 블록이 두개 있었고 삭제 블록이 시작이었다 */
        {
            PRED_FREE(SUCC_FREE(bp)) = NULL;
            segregation_list[list] = SUCC_FREE(bp);
        }
    }
    else                            /* 삭제 블록의 이후 블록이 없으면 */
    {
        if (PRED_FREE(bp) != NULL)  /* 삭제 블록의 이전 블록이 있으면 */
        {
            SUCC_FREE(PRED_FREE(bp)) = NULL;
        }
        else                        /* 삭제 블록의 이전 블록이 없으면 = 리스트에 삭제 블록 하나만 있었다 */
        {
            segregation_list[list] = NULL;
        }
    }
    return;
}

static void insert_block(void *bp, size_t size)
{
    int list = 0;
    void *search_ptr;
    void *insert_ptr = NULL;
    // 삽입해야 할 블록의 segregation list 위치 탐색
    while ((list < LISTLIMIT - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }
    // 삽입될 위치 탐색의 시작점
    search_ptr = segregation_list[list];
    // 가용 리스트 사이의 삽입 위치 탐색, insert_ptr후, search_ptr전
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr))))
    {
        insert_ptr = search_ptr;
        search_ptr = SUCC_FREE(search_ptr);
    }
    // 삽입 위치에 따른 삽입 블록의 연결 경우 - 리스트 중간으로, 맨 앞으로, 맨 뒤로, 처음으로 
    if (search_ptr != NULL)
    {
        if (insert_ptr != NULL)
        {
            SUCC_FREE(bp) = search_ptr;
            PRED_FREE(bp) = insert_ptr;
            PRED_FREE(search_ptr) = bp;
            SUCC_FREE(insert_ptr) = bp;
        }
        else
        {
            SUCC_FREE(bp) = search_ptr;
            PRED_FREE(bp) = NULL;
            PRED_FREE(search_ptr) = bp;
            segregation_list[list] = bp;
        }
    }
    else
    {
        if (insert_ptr != NULL)
        {
            SUCC_FREE(bp) = NULL;
            PRED_FREE(bp) = insert_ptr;
            SUCC_FREE(insert_ptr) = bp;
        }
        else
        {
            SUCC_FREE(bp) = NULL;
            PRED_FREE(bp) = NULL;
            segregation_list[list] = bp;
        }
    }

    return;
}

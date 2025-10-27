/*
 * mm-naive.c - Implicit free list allocator (basic, first-fit + split + coalesce)
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* 기본 상수 및 매크로 정의 */
#define WSIZE       4             /* 워드 크기 (header/footer) */
#define DSIZE       8             /* 더블 워드 크기 */
#define CHUNKSIZE  (1<<12)        /* heap 확장 크기 = 4KB */
#define MAX(x, y) ((x) > (y)? (x) : (y))

/* size와 alloc bit을 하나의 워드로 묶음 */
#define PACK(size, alloc)  ((size) | (alloc))

/* 주소 p가 가리키는 워드 읽기/쓰기 */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* header/footer에서 크기/할당비트 읽기 */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 블록 포인터 bp 기준 header/footer 계산 (bp는 payload 포인터) */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 이전/다음 블록의 payload 포인터 */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* 8바이트 정렬 */
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char *heap_listp = 0;  /* 프롤로그 블록의 payload를 가리킴 */

team_t team = {
    "teamFive",
    "GaramKim",
    "kimgalam0831@gmail.com",
    "KraftonJungle",
    "Jungle@SoSexy.com"
};

static void *extend_heap(size_t words);
static void *coalesce(char *bp);

/* 병합 */
static void* coalesce(char *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {                 /* Case 1 */
        return bp;
    }
    else if (prev_alloc && !next_alloc) {           /* Case 2: next free */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return bp;
    }
    else if (!prev_alloc && next_alloc) {           /* Case 3: prev free */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    /* 새 헤더는 prev의 헤더 */
        PUT(FTRP(bp), PACK(size, 0));               /* 새 푸터는 현재 bp의 푸터 위치 */
        return PREV_BLKP(bp);
    }
    else {                                          /* Case 4: both free */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    /* 앞 블록의 헤더 */
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));    /* 뒤 블록의 푸터 */
        return PREV_BLKP(bp);
    }
}

/* 힙 확장 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* alignment 유지를 위해 짝수 개수의 words를 allocate */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* 새 free 블록의 헤더/푸터, 그리고 새 에필로그 설정 */
    PUT(HDRP(bp), PACK(size, 0));          /* Free block header   (bp는 payload 포인터로 취급) */
    PUT(FTRP(bp), PACK(size, 0));          /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  /* New epilogue header */

    /* 직전 블록이 free였다면 병합 */
    return coalesce(bp);
}

/* 힙 초기화 */
int mm_init(void)
{
    /* 프롤로그(8B, alloc=1) + 에필로그(0, alloc=1) + 패딩 4B = 총 16B */
    if ((heap_listp = mem_sbrk(WSIZE * 4)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                            /* 패딩 */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));       /* 프롤로그 헤더 */
    PUT(heap_listp + DSIZE, PACK(DSIZE, 1));       /* 프롤로그 푸터 */
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));         /* 에필로그 헤더 */
    heap_listp += (2*WSIZE);                       /* 프롤로그 payload 가리키도록 */

    /* 초기 힙 확장 */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/* malloc */
void *mm_malloc(size_t size)
{
    size_t reqsize;      /* 조정된 요청 크기 (헤더/푸터 포함, 8바이트 정렬) */
    char *bp;

    if (size == 0) return NULL;

    /* 최소 블록 크기(16B) 보장 */
    if (size <= DSIZE)
        reqsize = 2 * DSIZE;
    else
        reqsize = ALIGN(size + 2 * WSIZE);

    /* first-fit 탐색 */
    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= reqsize) {
            size_t blockSize = GET_SIZE(HDRP(bp));
            if (blockSize - reqsize >= 2 * DSIZE) {
                /* 분할 */
                PUT(HDRP(bp), PACK(reqsize, 1));
                PUT(FTRP(bp), PACK(reqsize, 1));

                char *next_bp = NEXT_BLKP(bp);
                PUT(HDRP(next_bp), PACK(blockSize - reqsize, 0));
                PUT(FTRP(next_bp), PACK(blockSize - reqsize, 0));
            } else {
                /* 통짜 할당: 반드시 blockSize로 기록 */
                PUT(HDRP(bp), PACK(blockSize, 1));
                PUT(FTRP(bp), PACK(blockSize, 1));
            }
            return bp;
        }
    }

    /* 못 찾으면 힙 확장 후 배치 */
    bp = extend_heap(MAX(reqsize, CHUNKSIZE) / WSIZE);
    if (bp == NULL) return NULL;

    /* 확장한 블록에 배치 (분할 규칙 동일 적용) */
    {
        size_t blockSize = GET_SIZE(HDRP(bp));
        if (blockSize - reqsize >= 2 * DSIZE) {
            PUT(HDRP(bp), PACK(reqsize, 1));
            PUT(FTRP(bp), PACK(reqsize, 1));

            char *next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK(blockSize - reqsize, 0));
            PUT(FTRP(next_bp), PACK(blockSize - reqsize, 0));
        } else {
            PUT(HDRP(bp), PACK(blockSize, 1));
            PUT(FTRP(bp), PACK(blockSize, 1));
        }
    }
    return bp;
}

/* free */
void mm_free(void *ptr)
{
    if (ptr == NULL) return;

    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce((char *)ptr);
}

/* realloc (단순 구현) */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0)   { mm_free(ptr); return NULL; }

    void *newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE;   /* payload 크기 */
    if (size < copySize) copySize = size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}

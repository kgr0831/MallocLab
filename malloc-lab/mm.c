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

/* header/footer에서 크기와 할당 상태 읽기 */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 블록 포인터 bp 기준 header/footer 계산 */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 다음/이전 블록 포인터 계산 */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *heap_listp = 0;  /* 항상 heap의 첫 번째 블록을 가리킴 (전역 힙) */

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = 
{
    /* Team name */
    "teamFive",
    /* First member's full name */
    "GaramKim",
    /* First member's email address */
    "kimgalam0831@gmail.com",
    /* Second member's full name (leave blank if none) */
    "KraftonJungle",
    /* Second member's email address (leave blank if none) */
    "Jungle@SoSexy.com"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char* heapStartPos = NULL;
static char* heapEndPos = NULL;
/*
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    mem_sbrk(WSIZE * 4); // 16byte 시작 (CSAPP 권장)
    heapStartPos = mem_heap_lo(); // 시작 바이트 주소
    heapEndPos = mem_heap_hi(); // 끝나는 바이트 주소
    PUT(heap_listp, 0); // put = 4바이트 할당 -> 패딩으로 0 설정해서 채움.
    // 패딩 = 시작점을 나타내주면서 이 뒤로는 8바이트로 정렬해야함을 표시
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); // 프롤로그 헤더에 정보 입력 -> 8바이트 크기(=DSIZE) 블록을 사용 중(=1)
    PUT(heap_listp + DSIZE, PACK(DSIZE, 1)); // 프롤로그 푸터에 정보 입력 -> 8바이트 크기(=DSIZE) 블록을 사용 중(=1)
    PUT(heap_listp + (WSIZE * 3), PACK(0,1));
    // 에필로그 헤더: 크기(size)=0, 할당 상태(alloc)=1
    // 실제로는 4바이트짜리 헤더만 존재하며, 힙의 끝을 표시하는 표지판 역할을 한다.
    // 크기가 0이라는 뜻은 "이 뒤로는 어떤 블록도 없음"을 의미한다.
    
    // 프롤로그와 에필로그는 둘 다 사용자에게 할당되지 않는, 
    // 내부 관리용 블록이다.
    // 프롤로그는 힙의 시작 경계를 표시하는 더미 블록 (size=8, alloc=1)
    // 에필로그는 힙의 끝 경계를 표시하는 더미 헤더 (size=0, alloc=1)
    // 두 블록은 형태는 다르지만 공통적으로 병합 과정에서
    // 경계를 안전하게 처리하기 위한 보호벽 역할을 한다.

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
    {
        return NULL;
    }
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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
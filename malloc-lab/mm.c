#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/** @brief 워드 사이즈 (4바이트) */
#define WSIZE 4

/** @brief 더블 워드 사이즈 (8바이트) */
#define DSIZE 8

/** @brief 힙 확장 시 기본 크기 (4KB) */
#define CHUNKSIZE (1 << 12)

/** @brief 두 인자 중 큰 값을 반환 */
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/** 
 * @brief 크기와 할당 여부(0 또는 1)를 하나의 워드로 묶음 
 * @param size 블록 크기
 * @param alloc 할당 여부 (1 = 할당됨, 0 = 비할당)
 */
#define PACK(size, alloc) ((size) | (alloc))

/** @brief 주소 p가 가리키는 워드 읽기 */
#define GET(p) (*(unsigned int *)(p))

/** @brief 주소 p가 가리키는 워드에 값 쓰기 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/** @brief 헤더나 푸터에서 블록 크기 추출 */
#define GET_SIZE(p) (GET(p) & ~0x7)

/** @brief 헤더나 푸터에서 할당 여부 추출 */
#define GET_ALLOC(p) (GET(p) & 0x1)

/** 
 * @brief 블록 포인터 bp 기준 헤더 주소 반환
 * @note bp는 payload의 시작 주소
 */
#define HDRP(bp) ((char *)(bp) - WSIZE)

/**
 * @brief 블록 포인터 bp 기준 푸터 주소 반환
 * @note bp는 payload의 시작 주소
 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/** @brief 다음 블록의 payload 포인터 반환 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))

/** @brief 이전 블록의 payload 포인터 반환 */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/** @brief 정렬 기준 크기 (8바이트 단위) */
#define ALIGNMENT 8

/** @brief 주어진 size를 8바이트 단위로 정렬 */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/** @brief size_t 타입 크기를 8바이트로 정렬한 값 */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/** @brief 프롤로그(payload 시작 부분)를 가리키는 전역 포인터 */
static char *heap_listp = 0;

/** 
 * @brief 팀 정보 구조체 (malloc lab 제출용)
 */
team_t team = {
    "teamFive",               /**< 팀 이름 */
    "GaramKim",               /**< 팀원 이름 */
    "kimgalam0831@gmail.com", /**< 팀원 이메일 */
    "KraftonJungle",          /**< 소속 */
    "Jungle@SoSexy.com"       /**< 추가 멤버 또는 식별 정보 */
};


/// @brief 힙을 확장해주는 함수
/// @param words 워드의 갯수(예 : 24바이트 -> 6)
/// @return 확장한 부분의 첫 바이트 주소 bp
static void *extend_heap(size_t words);

/// @brief 빈 블록들을 병합해주는 함수
/// @param bp 병합의 기준이 될 빈 블록
/// @return 병합한 블록의 payload 포인터
static void *coalesce(char *bp);

static void* coalesce(char *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) // 앞 뒤에 빈 블록이 없을 때
    {
        return bp;
    }
    else if (prev_alloc && !next_alloc) // 뒤에 블록만 비어있을 때
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return bp;
    }
    else if (!prev_alloc && next_alloc) // 앞의 블록만 비어있을 때
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return PREV_BLKP(bp);
    }
    else // 앞과 뒤 블록 다 비어있을 때
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        return PREV_BLKP(bp);
    }
}

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

/// @brief 힙을 초기화 하는 함수
/// @param  
/// @return 성공 여부 : -1(fail) / 0(success)
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(WSIZE * 4)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0); // 패딩
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); // 프롤로그 헤더
    PUT(heap_listp + DSIZE, PACK(DSIZE, 1)); // 프롤로그 푸터
    PUT(heap_listp + 3*WSIZE, PACK(0, 1)); // 에필로그 헤더
    heap_listp += (2*WSIZE); // 프롤로그 payload 가리키도록 바꾸기

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // 힙 확장 -> 4KB
    {
        return -1; // 힙 확장 실패
    }

    return 0; // 힙 확장 성공
}

/// @brief malloc 함수
/// @param size 할당 받을 크기
/// @return 할당 받을 블록의 payload
void *mm_malloc(size_t size)
{
    size_t reqsize; // 요구 받은 사이즈를 정렬한 사이즈
    char *bp; // 리턴할 블록 payload의 포인터

    if (size == 0) // 요구 사이즈 = 0
    {
        return NULL;
    }

    reqsize = size <= DSIZE ? 2 * DSIZE :  ALIGN(size + 2 * WSIZE); // 요구 사이즈를 정렬
    // 요구 사이즈가 DSIZE보다 작거나 같으면 최소 16바이트로 정렬, 아니면 WSIZE의 배수로 정렬

    for (bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= reqsize) 
        {
            size_t blockSize = GET_SIZE(HDRP(bp));
            if (blockSize - reqsize >= 2 * DSIZE) 
            {
                PUT(HDRP(bp), PACK(reqsize, 1));
                PUT(FTRP(bp), PACK(reqsize, 1));

                char *next_bp = NEXT_BLKP(bp);
                PUT(HDRP(next_bp), PACK(blockSize - reqsize, 0));
                PUT(FTRP(next_bp), PACK(blockSize - reqsize, 0));
            }
            else 
            {
                PUT(HDRP(bp), PACK(blockSize, 1));
                PUT(FTRP(bp), PACK(blockSize, 1));
            }
            return bp;
        }
    }

    bp = extend_heap(MAX(reqsize, CHUNKSIZE) / WSIZE);
    if (bp == NULL)
    {
        return NULL;
    }
    size_t blockSize = GET_SIZE(HDRP(bp));
    if (blockSize - reqsize >= 2 * DSIZE) 
    {
        PUT(HDRP(bp), PACK(reqsize, 1));
        PUT(FTRP(bp), PACK(reqsize, 1));

        char *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(blockSize - reqsize, 0));
        PUT(FTRP(next_bp), PACK(blockSize - reqsize, 0));
    } 
    else 
    {
        PUT(HDRP(bp), PACK(blockSize, 1));
        PUT(FTRP(bp), PACK(blockSize, 1));
    }

    return bp;
}

void mm_free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce((char *)ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }

    if (size == 0)
    {
        mm_free(ptr);
        return NULL; 
    }

    void *newptr = mm_malloc(size);
    if (newptr == NULL)
    {
        return NULL;
    }

    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < copySize)
    {
        copySize = size;
    }
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}
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
// size -> 크기
// alloc -> allocated bit(할당여부 비트)
// 0x10 | 0x0 -> 0x10
// 0x10 | 0x1 -> 0x11

/* 주소 p가 가리키는 워드 읽기 */
#define GET(p)       (*(unsigned int *)(p))
/* 주소 p가 가리키는 워드 쓰기 */
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* header/footer에서 크기 읽기 */
#define GET_SIZE(p)  (GET(p) & ~0x7)
/* header/footer에서 할당 상태(0 or 1) 읽기 */
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 블록 포인터 bp 기준 header 계산 */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
/* 블록 포인터 bp 기준 footer 계산 */
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 다음 블록 포인터 계산 */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
/* 이전 블록 포인터 계산 */
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

/* 올림의 기준이 되는 수 */
#define ALIGNMENT 8

/* 8의 배수로 올림(예 : 9 -> 16) */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

const static char* prologueFooter;
static char* EpilogueHeader;

/*
 * mm_init - initialize the malloc package.
 */

/* 힙 생성 및 초기화 함수 - return -1 : 실패 / return 0 : 성공 */
int mm_init(void)
{
    if((heap_listp = mem_sbrk(WSIZE * 4)) == (void *)-1) // 16byte 시작 (CSAPP 권장)
    {
        return -1;
    }
    PUT(heap_listp, 0); // put = 4바이트 할당 -> 패딩으로 0 설정해서 채움.
    // 패딩 = 시작점을 나타내주면서 이 뒤로는 8바이트로 정렬해야함을 표시
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1)); // 프롤로그 헤더에 정보 입력 -> 8바이트 크기(=DSIZE) 블록을 사용 중(=1)
    prologueFooter = PUT(heap_listp + DSIZE, PACK(DSIZE, 1)); // 프롤로그 푸터에 정보 입력 -> 8바이트 크기(=DSIZE) 블록을 사용 중(=1)
    EpilogueHeader = PUT(heap_listp + (WSIZE * 3), PACK(0,1));
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

/*malloc 함수 - size : 할당할 크기*/
void *mm_malloc(size_t size)
{
    // 1. 사용자가 원하는 크기를 받는다.
    // 2. 그 크기가 남은 힙의 크기 보다 작으면 
    // 그냥 할당해주고 크기를 나타내주는 변수를 줄인다.
    // -> 할당은 원하는 크기 + a(패딩)를 통해 8의 배수가 크기인 하나의 블록으로 해준다.
    // 3. 만약에 힙의 크기가 요청 크기보다 작으면 힙의 크기를 늘려주며
    // 기존 에필로그 헤더를 Put()을 통해 뒤로 당겨주고 크기를 나타내주는
    // 변수에 늘린 크기만큼 더해준다음에 할당해주고 크기를 나타내주는 변수를 줄인다.
    // -> 할당은 원하는 크기 + a(패딩)를 통해 8의 배수가 크기인 하나의 블록으로 해준다.
    
    int reqsize = ALIGN(size + SIZE_T_SIZE); // 8바이트 배수로 정렬
    char* curBlockPointer = NEXT_BLKP(prologueFooter);
    while(GET_SIZE(curBlockPointer) > 0)
    {
        if(!GET_ALLOC(HDRP(curBlockPointer)) && (GET_SIZE(curBlockPointer) >= reqsize))
        {
            PUT(HDRP(curBlockPointer), PACK(reqsize, 1)); // 헤더 설정
            PUT(FTRP(curBlockPointer), PACK(reqsize, 1)); // 푸터 설정
            return curBlockPointer;
        }
        curBlockPointer = NEXT_BLKP(curBlockPointer);
    }

     // 힙의 크기를 CHUNKSIZE 만큼 확장
    // if (p == (void *)-1) // 만약 확장 실패시
    // {
    //     return NULL; // NULL 반환
    // }
    // else
    // {
    //     PUT(EpilogueHeader, 0);
    //     EpilogueHeader = PUT(p, PACK(0,1));
    //     *(size_t*)p = size;
    //     // 사용자가 요청한 크기를 할당된 블록의 맨 앞 8바이트인 헤더에 채움.
    //     return (void *)((char *)p + SIZE_T_SIZE); // 사용자에게 준 블록의 payloads 까지
    // }
}


/// @brief 빈 블록들을 연결시키는 함수
/// @param p 합칠 블록 중 하나
/// @return 합친 블록의 포인터
void* coalesce(char *p)
{
    char* frontBlock = FTRP(PREV_BLKP(p));
    char* backBlock = HDRP(NEXT_BLKP(p));
    size_t size = GET_SIZE(FTRP(p));
    if(GET_ALLOC(frontBlock) && GET_ALLOC(backBlock)) // 아무것도 안빔 -> 아무것도 그냥 하지마
    {
       return p;
    }
    else // 무언가가 빌 때
    {
        if(GET_ALLOC(frontBlock))  // 뒤만 빔 -> 지금 + 뒤 합침
        {
            size += GET_SIZE(backBlock);
            PUT(HDRP(p), PACK(size, 0)); 
            PUT(FTRP(p), PACK(size,0));

            // 헤더와 푸터에 size를 갱신
        }
        else if(GET_ALLOC(backBlock)) // 앞만 빔 -> 앞 + 지금 합임
        {
            size += GET_SIZE(frontBlock);
            PUT(FTRP(p), PACK(size, 0));
            PUT(HDRP(PREV_BLKP(p)), PACK(size, 0));
            p = PREV_BLKP(p);
        }
        else  // 앞 뒤 둘다 빔 -> 앞 + 지금 + 뒤 합침
        {
            size += (GET_SIZE(frontBlock) + GET_SIZE(backBlock));
            PUT(HDRP(PREV_BLKP(p)), PACK(size,0));
            PUT(FTRP(NEXT_BLKP(p)), PACK(size,0));
            // 각각 헤더가 앞의 헤더, 푸터가 뒤의 푸터
            p = PREV_BLKP(p);
        }

        return p;
    }
}

/* 새 가용 블록으로 힙 확장, 예시로는 2의 10승만큼 워드블록을 만들어라.*/ 
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size; // size+t = unsigned int, 이 함수에서 넣을 size를 하나 만들어줌.
    /* alignment 유지를 위해 짝수 개수의 words를 Allocate */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 홀수이면(1이나오니까) 앞에꺼, 짝수이면(0이 나오니까) 뒤에꺼. 홀수 일 때 +1 하는건 8의 배수 맞추기 위함인듯.
    // 홀수가 나오면 사이즈를 한번 더 재정의. 힙에서 늘릴 사이즈를 재정의.
    if ( (long)(bp = mem_sbrk(size)) == -1)
    { // sbrk로 size로 늘려서 long 형으로 반환한다.(한번 쫙 미리 늘려놓는 것) mem_sbrk가 반환되면 bp는 현재 만들어진 블록의 끝에 가있음.
        return NULL;
    } // 사이즈를 늘릴 때마다 old brk는 과거의 mem_brk위치로 감.

    /* free block 헤더와 푸터를 init하고 epilogue 헤더를 init*/
    PUT(HDRP(bp), PACK(size,0)); // free block header 생성. /(prologue block이랑 헷갈리면 안됨.) regular block의 총합의 첫번째 부분. 현재 bp 위치의 한 칸 앞에 헤더를 생성.
    PUT(FTRP(bp),PACK(size,0)); // free block footer / regular block 총합의 마지막 부분.
    EpilogueHeader = PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // 블록을 추가했으니 epilogue header를 새롭게 위치 조정해줌. (HDRP: 1워드 뒤에 있는 것을 지칭. 
    // 처음 세팅의 의미 = bp를 헤더에서 읽은 사이즈만큼 이동하고, 앞으로 한칸 간다. 그위치가 결국 늘린 블록 끝에서 한칸 간거라 거기가 epilogue header 위치.

    /* 만약 prev block이 free였다면, coalesce해라.*/
    return coalesce(bp); // 처음에는 coalesc를 할게 없지만 함수 재사용을 위해 리턴값으로 선언
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


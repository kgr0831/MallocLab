#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


void* start = NULL;
void* end = NULL;
size_t size = 0;
size_t left = 0;

int heapSizeUp(size_t inc) // 메모리의 크기는 int가 아닌 size_t로 해야함 -> int로 하면 넘칠수도
{
    if (inc == 0) return 0;

    inc = (inc + 7) & ~(size_t)7; // 8바이트의 배수로 올림

    char *pos = mem_sbrk((int)inc); // 확장한거 시작위치
    // char가 1바이트라 바이트 단위 주소 연산 가능하고, void*는 연산이 불가능함, 
    // 또한 어떤 객체든 다 char*로 접근 가능
    if (pos == (void*)-1) return -1; // 힙 확장 실패시 -1 반환

    if (!start) start = pos;
    end = pos + inc;
    size += inc;
    left += inc;
    return 0;
}

void* MyMalloc(int kSize)
{
    if(kSize > left)
    {
        heapSizeUp(kSize - left);
    }
    void* ptr = start + (size - left);
    left -= kSize;
    return ptr;
}

void MyFree(void* kPtr)
{
    
}

int main(void)
{
    mem_init();
    int* ptr = MyMalloc(sizeof(int));
    *ptr = 99;
    printf("%p",ptr);
    printf("\n================================\n");
    printf("%d",*ptr);
    return 0;
}
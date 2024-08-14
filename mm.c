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
//#define ALIGNMENT 8
/*시스템에 맞게 사이즈 할당*/
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8

//* rounds up to the nearest multiple of ALIGNMENT */
//#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
/*ALIGNMENT의 값에 맞게 사이즈 할당*/
#define ALIGN(size) (((size) + (ALIGNMENT-1)) &  ~(ALIGNMENT - 1))

#define MAX(x,y) ((x) > (y)? (x) : (y))

#define PACK(size,alloc) ((size) | (alloc))

//8의 배수로 변환

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//변환한 8의 배수 사이즈를 설정

#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//헤더포인터 주소
#define HDRP(bp) ((char *)(bp) - WSIZE)
//푸터포인터 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//다음과 이전 주소 계산
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

// Global variables
static void *heap_listp = 0;  // 힙의 시작 포인터
static void *next_bp;
#define CHUNKSIZE (1<<12) // 힙 확장의 기본 크기 4KB

// extend_heap 전방 선언
static void *extend_heap(size_t words);
// coalesce 전방 선언
static void *coalesce(void *bp);
// find_fit 전방 선언
static void *find_fit(size_t asize);
// place 전방 선언
static void place(void *bp, size_t asize);
static void *next_fit(size_t asize);
static void *next_fit_coalesce(void *bp);
static void *delay_next_fit_coalesce(void);
static void *next_after_best_fit(size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1)
        return -1;

    PUT(heap_listp , 0); //Alignment padding
    PUT(heap_listp + (1 * WSIZE) , PACK(DSIZE,1)); //Prologue header
    PUT(heap_listp + (2 * WSIZE) , PACK(DSIZE,1)); //Prologue footer
    PUT(heap_listp + (3 * WSIZE) , PACK(0,1)); //Epilogue header
    heap_listp += (2 * WSIZE);

    next_bp = heap_listp;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    return 0;
}

/*
 * extend_heap - 힙을 확장하고 새로운 빈 블록을 생성하는 함수
 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    //홀수면 +1하고 WSIZE만큼, 짝수면 WSIZE만큼
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp),PACK(size,0)); //Free Block Header
    PUT(FTRP(bp),PACK(size,0)); //Free Block Footer
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1)); //New Epilogue Header

    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendSize;
    char *bp;

    if(size == 0)
        return NULL;

    //size가 8이하면 16리턴(헤더+푸터가 8바이트,나머지는 8바이트 단위)
    if(size <= DSIZE)
        asize = 2 * DSIZE;
    //헤더푸터(8바이트) 추가 후 8의배수 올림
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    //크기에 맞는 가용리스트 검색
    if((bp = next_after_best_fit(asize)) != NULL) {
        //블록 분할 후 리턴
        place(bp,asize);
        return bp;
    }

    //NULL값이 나온다면(가용리스트가 없다면) 이때부터 병합실행
    delay_next_fit_coalesce();

    //병합후에도 안되는지 설정
    if((bp = find_fit(asize)) != NULL) {
        //블록 분할 후 리턴
        place(bp,asize);
        return bp;
    }

    extendSize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendSize/WSIZE)) == NULL)
        return NULL;
    place(bp,asize);
    return bp;


    /*
    //brk pointer(break pointer,힙 메모리 시작주소)
    //newsize는 기존 size에 8을 더한다
    int newsize = ALIGN(size + SIZE_T_SIZE);
    //mem_sbrk=>성공시(멀쩡한 값을 넣고,공간할당량을 넘어가지 않았다면) 새로 할당한 메모리의 시작주소(이전 메모리의 끝 주소) 반환
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
    */
}

/*
 * find_fit - 프리 리스트에서 할당할 수 있는 블록을 찾는 함수
 */
static void *find_fit(size_t asize) {
    void *bp;

    /*
    // 최초 적합(first-fit) 전략으로 블록 탐색
    for (bp = heap_listp; *(size_t *)bp != 1; bp = (char *)bp + (*(size_t *)bp & ~0x7)) {
        if (!(*(size_t *)bp & 1) && (asize <= (*(size_t *)bp & ~0x7))) {
            return bp;
        }
    }*/
    
    //힙 시작부분부터 에필로그 헤더 전까지 검색
    for(bp = heap_listp;GET_SIZE(HDRP(bp)) > 0;bp = NEXT_BLKP(bp)){
        //가용상태 + 요청 사이즈 이상이면 리턴
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;
    }
    
    return NULL;
}

 static void *next_after_best_fit(size_t asize){
    void *bp;
    void *best_bp = NULL;
    size_t bestSize = 0;

    for(bp = next_bp ; GET_SIZE(HDRP(bp)) > 0 ; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            next_bp = bp;
            return bp;
        }
    }

    for(bp = heap_listp;bp < next_bp;bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            if(best_bp == NULL){
                bestSize = (GET_SIZE(HDRP(bp)) - asize);
                best_bp = bp;
            }

            if(bestSize > (GET_SIZE(HDRP(bp))- asize)){
                bestSize = GET_SIZE(HDRP(bp))- asize;
                best_bp = bp;
            }
        }
    }

    return best_bp;
 }

/*
 * place - 할당된 블록을 프리 리스트에서 제거하고, 남은 공간을 프리 블록으로 변환하는 함수
 */
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    //할당후 남은 가용블록이 최소블록 기준(16바이트) 이상이면 분할
    if((csize - asize) >= (2 * DSIZE)){
        //할당할 가용 블록의 헤더/푸터 설정
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));

        //할당하고 남은 블록의 헤더/푸터 설정
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize - asize,0));
        PUT(FTRP(bp),PACK(csize - asize,0));
    }
    //남은 가용 블록이 16보다 작을 경우,남은것을 할당한다
    else{
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
}

/*
 * coalesce - 빈 블록을 병합하는 함수
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    //case1 양쪽 다 할당
    if(prev_alloc && next_alloc){
        return bp;
    }
    //case2 앞쪽 할당 뒷쪽 가용
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
        //next_bp 할당
        if (next_bp >= HDRP(bp) && next_bp <= FTRP(bp))
            next_bp = bp;
    }
    //case3 앞쪽 가용 뒷쪽 할당
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        if (next_bp >= HDRP(bp) && next_bp <= FTRP(bp))
            next_bp = PREV_BLKP(bp);
        bp = PREV_BLKP(bp);
    }
    //case4 양쪽 다 가용
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        if (next_bp >= HDRP(bp) && next_bp <= FTRP(bp))
            next_bp = PREV_BLKP(bp);
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *delay_next_fit_coalesce(void){
    void *bp;

    //bp시작부터 에필로그까지 순회
    for(bp = heap_listp;GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0; bp = NEXT_BLKP(bp)){
        //가용상태라면
        if(!GET_ALLOC(HDRP(bp))){
            //다음블록들이 할당상태가 나오기전까지
            while (!GET_ALLOC(HDRP(NEXT_BLKP(bp))))
            {
                //병합한다
                size_t size = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
                PUT(HDRP(bp),PACK(size,0));
                PUT(FTRP(bp),PACK(size,0));
            }
            next_bp = bp;
        }
    }
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *ptr, size_t size)
{
    size_t old_size = GET_SIZE(HDRP(ptr));  // 기존 블록의 크기
    size_t asize = ALIGN(size + DSIZE);     // 새로 할당할 크기 (헤더, 푸터 포함)
    
    // 새로운 크기가 기존 크기보다 작거나 같으면 그대로 사용
    if (asize <= old_size) {
        return ptr;
    }

    void *prev_blk = PREV_BLKP(ptr);
    void *next_blk = NEXT_BLKP(ptr);
    // 블록 병합 시도: 다음 블록이 가용한 경우
    if(!GET_ALLOC(HDRP(next_blk)) && (old_size + GET_SIZE(HDRP(next_blk)) >= asize)) {
        size_t new_size = old_size + GET_SIZE(HDRP(next_blk));
        PUT(HDRP(ptr), PACK(new_size, 1));
        PUT(FTRP(ptr), PACK(new_size, 1));
        return ptr;
    }
    else if (!GET_ALLOC(HDRP(prev_blk)) && (old_size + GET_SIZE(HDRP(prev_blk)) >= asize)) {
        size_t prev_size = GET_SIZE(HDRP(prev_blk));
        size_t new_size = old_size + prev_size;

        // prev_blk와 ptr이 구분되는지 확인
        if (prev_blk != ptr && prev_blk + prev_size == ptr) {
            // 이전 블록을 확장하여 현재 블록과 병합
            PUT(HDRP(prev_blk), PACK(new_size, 1));
            PUT(FTRP(prev_blk), PACK(new_size, 1));

            // 메모리 영역이 겹치는 경우를 대비하여 memmove 사용
            memmove(prev_blk, ptr, old_size);

            // 기존 블록 해제 (이미 병합되었으므로 사실상 필요하지 않음)
            mm_free(ptr);

            return prev_blk;  // 새로운 위치 반환
        }
    }

    // 위의 방법으로도 충분히 확장할 수 없는 경우, 새로운 블록을 할당
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    // 기존 데이터를 새로운 블록으로 복사하고, 기존 블록 해제
    memcpy(new_ptr, ptr, old_size - DSIZE);
    mm_free(ptr);
    return new_ptr;
}















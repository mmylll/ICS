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
        "19302010058",
        /* First member's full name */
        "李正阳",
        /* First member's email address */
        "19302010058@fudan.edu.cn",
        /* Second member's full name (leave blank if none) */
        "",
        /* Second member's email address (leave blank if none) */
        ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* Align size to double words */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
// P 599 课本定义的
// 基本常数和宏
#define WSIZE 4				// 字、头、尾大小
#define DSIZE 8				// 双字大小
#define CHUNKSIZE (1<<12)	// 扩展堆
#define MAXCLASS 15			// class 最大数量

#define MAX(x, y) ((x) > (y)? (x) : (y)) //两数最大值

// 将大小和分配的位打包
#define PACK(size, alloc) ((size) | (alloc))

// 在p处读写
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define PUT_P(p, p_to) (*(unsigned int *)(p) = (unsigned int)(p_to))

// 从p处读取大小和分配的字段
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// 给出块，算出其头尾地址
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 给出块计算前一个和后一个块地址
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// 给出块，计算其前驱和后继指针
#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + WSIZE)

// 给出块，计算其前驱和后继块地址
#define PRED(bp) (*(char **)(bp))
#define SUCC(bp) (*(char **)(SUCC_PTR(bp)))

// 大小类链表指针
void* heap_listp;

/*
 * 将ptr bp插入到分离的空闲链表中的适当位置
 */
static void insert_list(void* bp, size_t size)
{
    int class_idx = 0;										//大小类索引
    void* class_ptr, * current_ptr, * last_ptr = NULL;		//大小类指针

    //搜索链表中符合大小的索引
    while ((size > 1) && (class_idx < MAXCLASS - 1)) {
        size >>= 1;
        class_idx ++;
    }
    class_ptr = heap_listp + class_idx * WSIZE;
    current_ptr = (void*)GET(class_ptr);

    // 搜索插入的位置
    while (current_ptr != NULL && (size > GET_SIZE(HDRP(current_ptr)))) {
        last_ptr = current_ptr;
        current_ptr = SUCC(current_ptr);
    }


    /* 插入的情况
     1: 插入到中间, last_ptr -> bp -> current_ptr
     2: 插入到头, class_ptr (bp) -> current_ptr
     3: 插入到尾, last_ptr -> bp -> current_ptr (NULL)
     4: 空的链表, class_ptr (bp) -> current_ptr (NULL)
     */
    PUT_P(PRED_PTR(bp), last_ptr);
    PUT_P(SUCC_PTR(bp), current_ptr);

    if (current_ptr != NULL)
        PUT_P(PRED_PTR(current_ptr), bp);

    if (last_ptr != NULL)
        PUT_P(SUCC_PTR(last_ptr), bp);
    else
        PUT_P(class_ptr, bp);
}

/*
 * 从分离空闲链表中移除ptr bp
 */
static void remove_list(void* bp)
{
    int class_idx = 0;
    void* class_ptr;
    size_t size = GET_SIZE(HDRP(bp));

    //搜索链表中符合大小的索引
    while ((size > 1) && (class_idx < MAXCLASS - 1)) {
        size >>= 1;
        class_idx ++;
    }
    class_ptr = heap_listp + class_idx * WSIZE;

    /* 移除的情况
     1: 从中间移除
     2: 从头部移除
     3: 从尾部移除
     4: 链表中只有一个指针
     */
    if (PRED(bp) != NULL)
        PUT_P(SUCC_PTR(PRED(bp)), SUCC(bp));
    else PUT_P(class_ptr, SUCC(bp));

    if (SUCC(bp) != NULL)
        PUT_P(PRED_PTR(SUCC(bp)), PRED(bp));
}

/*
 * 如果相邻，合并空闲块
 */
static void* coalesce(void* bp, size_t size)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    if (prev_alloc && next_alloc);				// 1 : 前面的和后面的块都已分配

    else if (prev_alloc && !next_alloc) {		// 2 : 前面块已分配，后面块空闲
        remove_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {		// 3 : 前面块空闲，后面块已分配
        remove_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {										//  4 : 前面块和后面块都空闲
        remove_list(PREV_BLKP(bp));
        remove_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_list(bp, size);
    return bp;
}

/*
 * 初始化堆和mm_malloc不能找到合适大小的块时扩展堆
 */
static void* extend_heap(size_t size)
{
    void* bp;	// 块指针

    // 分配偶数个字来保持对齐
    size = ALIGN(size);
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // 初始化空闲块头部脚部
    PUT(HDRP(bp), PACK(size, 0));			// 空闲块头部
    PUT(FTRP(bp), PACK(size, 0));			// 空闲块脚部
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));	// 结尾块头部

    // 前一个块空闲时合并
    return coalesce(bp, size);
}

/*
 *  拆分块
 */
static void* place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)), remain = csize - asize;

    remove_list(bp);

    if (remain < (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    else if(asize < 96) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remain, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remain, 0));
        insert_list(NEXT_BLKP(bp), remain);
    }
    else {
        PUT(HDRP(bp), PACK(remain, 0));
        PUT(FTRP(bp), PACK(remain, 0));
        insert_list(bp, remain);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
    return bp;
}

/*
 *  在分离空闲链表找到对应大小的块实行分离适配
 */
static void* find_fit(size_t asize)
{
    int class_idx = 0;
    void* bp = NULL;
    size_t size = asize;

    // 首次适配搜索
    while (class_idx < MAXCLASS) {
        // 找到对应大小的链表索引
        if ((size <= 1) && ((bp = (void*)GET(heap_listp + class_idx * WSIZE)) != NULL)) {
            // 找到适合大小的块
            while ((bp != NULL) && (asize > GET_SIZE(HDRP(bp))))
                bp = SUCC(bp);
            if (bp != NULL)
                break;
        }

        size >>= 1;
        class_idx++;
    }
    return bp;
}

/*
 * mm_init - initialize prologue header, epilogue header and segregated list (size class list).
 */
int mm_init(void)
{
    int i;
    // 初始化堆
    if ((heap_listp = mem_sbrk((3 + MAXCLASS) * WSIZE)) == (void*)-1)
        return -1;

    // 初始化分离链表
    for (i = 0; i < MAXCLASS; i++)
        PUT_P(heap_listp + (i * WSIZE), NULL);

    PUT(heap_listp + (MAXCLASS * WSIZE), PACK(DSIZE, 1));		// 序言块头部
    PUT(heap_listp + ((1 + MAXCLASS) * WSIZE), PACK(DSIZE, 1));	// 序言块脚部
    PUT(heap_listp + ((2 + MAXCLASS) * WSIZE), PACK(0, 1));		// 结尾块头部

    // 用CHUNKSIZE大小的空闲块扩展堆
    if (extend_heap(CHUNKSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by finding a fit block. If no fit block, extend the heap.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;		// 调整的块大小
    size_t extendsize;	// 不适配扩展堆的大小
    char* bp;
    if (size == 0)
        return NULL;

    // 调整块大小来满足对齐和大小要求
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    // 搜索空闲链表来适配
    if ((bp = find_fit(asize)) != NULL)
        return place(bp, asize);

    // 没有找到块就申请额外内存或者拆分块
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize)) == NULL)
        return NULL;
    return place(bp, asize);
}

/*
 * mm_free - Freeing a block by setting its head and tail mark, then do coalesce if possible.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr, size);
}

/*
 * mm_realloc - Examine if nearest block is free. Coalesce them if the sum of their sizes
 *     is enough for realloc, extend heap otherwise.
 */
void* mm_realloc(void* ptr, size_t size)
{
    void* newptr = ptr;
    int remain;

    if (size == 0)
        return NULL;

    // 对齐
    if (size <= DSIZE)
        size = 2 * DSIZE;
    else
        size = ALIGN(size + DSIZE);

    if ((remain = GET_SIZE(HDRP(ptr)) - size) >= 0)
        return newptr;

    // 检查最近的块
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr)))) {
        // 不满足条件就扩展
        if ((remain = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - size) < 0) {
            if (extend_heap(MAX(-remain, CHUNKSIZE)) == NULL)
                return NULL;
            remain += MAX(-remain, CHUNKSIZE);
        }
        // 移除下一个块
        remove_list(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(size + remain, 1));
        PUT(FTRP(ptr), PACK(size + remain, 1));
    }
        //没有合适的空闲块
    else {
        newptr = mm_malloc(size);
        memcpy(newptr, ptr, GET_SIZE(HDRP(ptr)));
        mm_free(ptr);
    }

    return newptr;
}

/*
 * 检查块的正确性
 */
//void checkBlock(void *bp)
//{
//    if ((size_t)bp % 8) {
//        printf(" %p 不是双字对齐\n", bp);
//    }
//    if (GET(HDRP(bp)) != GET(FTRP(bp))) {
//        printf("头部与脚部不匹配\n");
//    }
//}

/*
 *检查堆一致性
 */
//void mm_check()
//{
//    void *bp = heap_listp;
//
//    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
//        printf("无效序言块头部\n");
//    }
//    checkBlock(heap_listp);
//
//    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
//        checkBlock(bp);
//    }
//
//    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
//        printf("无效结尾块头部\n");
//    }
//}

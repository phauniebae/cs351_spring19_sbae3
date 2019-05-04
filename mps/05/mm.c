p
/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define BLK_HDR_SIZE ALIGN(sizeof(blockHdr))
#define BLK_FTR_SIZE ALIGN(sizeof(blockFtr))
#define BLK_HDR_FTR_SIZE ALIGN(sizeof(blockHdr)) + ALIGN(sizeof(blockFtr)) 
#define GET_EPILOGUE (blockHdr *)(mem_heap_hi() - BLK_HDR_SIZE + 1)


typedef struct header blockHdr;
typedef struct footer blockFtr;


struct header {
	size_t size;
	blockHdr *next_p;
	blockHdr *prior_p;
};


struct footer {
	size_t size;
};


//define methods in the program
void *find_fit(size_t size);
void *map_to_list(int size);
void add_to_free_lists(blockHdr *head);
blockHdr *coalease(blockHdr *head);
blockHdr *split_block(int old_size, int new_size, blockHdr *head);
void remove_from_free_lists(blockHdr *head);
int mm_check();
int is_last_block(blockHdr *head);


//other constants and variables
static int NUM_OF_FREE_LISTS = 4;
blockHdr *LAST_LIST = NULL; //keep track of the very last list 




/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
	//in order to initialize, I will create the header which will point to free blocks of various sizes
	blockHdr *free_list_head = (blockHdr *) mem_sbrk(NUM_OF_FREE_LISTS * BLK_HDR_SIZE + BLK_FTR_SIZE);

	if(free_list_head < o){
		return -1;
	}

	//create the heads of the circularly linked double linked lists and set each list's previous pointer to itself
	
	int i;
	blockHdr *curr_free_list_head = free_ist_head;
	for(i=0; i<NUM_OF_FREE_LISTS; i++){
		curr_free_list_head -> size = 1;
		curr_free_list_head -> prior_p = curr_free_list_head;
		curr_free_list_head -> next+p = curr_free_list_head;
		LAST_LIST = curr_free_list_head; //keeps track of the last list for later use
		curr_free_list_head = (blockHdr *)((char *)curr_free_list_head + BLK_HDR_SIZE);//go to next block head
	}

	//set the footer that seperates the heads to allocated
	blockFtr *ftr = (blockFtr *)((char *)LAST_LIST + BLK_HDR_SIZE);
	ftr -> size = 1;

	//the epilogue is use to mark the end of the allocated blocks
	blockHdr *epilogue = mem_sbrk(BLK_HDR_SIZE);
	epilogue->size = BLK_HDR_SIZE | 1;
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
  if ((long)p == -1)
    return NULL;
  else {
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

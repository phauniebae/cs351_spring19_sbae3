
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
int mm_init(void){
	//in order to initialize, create the header which will point to free blocks 
	//of various sizes
	
	blockHdr *free_list_head = (blockHdr *) mem_sbrk(NUM_OF_FREE_LISTS * BLK_HDR_SIZE + BLK_FTR_SIZE);

	if(free_list_head < 0){
		return -1;
	}

	//create the heads of the circularly linked doubly linked lists 
	//and set each list's previous pointer to itself
	
	int i;
	blockHdr *curr_free_list_head = free_list_head;
	for(i=0; i<NUM_OF_FREE_LISTS; i++){
		curr_free_list_head -> size = 1;
		curr_free_list_head -> prior_p = curr_free_list_head;
		curr_free_list_head -> next_p = curr_free_list_head;
		LAST_LIST = curr_free_list_head; //keeps track of the last list for later use
		curr_free_list_head = (blockHdr *)((char *)curr_free_list_head + BLK_HDR_SIZE);//go to next block head
	}


	//set the footer that seperates the heads to allocated
	blockFtr *ftr = (blockFtr *)((char *)LAST_LIST + BLK_HDR_SIZE);
	ftr -> size = 1;


	//the epilogue is used to mark the end of the allocated blocks
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
	//if we are given zero size, then there is nothing to malloc
	if (size == 0) return NULL;
	
	int new_size = ALIGN(size + BLK_HDR_FTR_SIZE);
	
	//check if there's a free block that will contain that size
	blockHdr *free_block = find_fit(new_size);

	if (free_block == NULL ){
		//if there's no free block, we create one 
		//keep track of epilogue before calling sbrk()
	
		blockHdr *epilogue = GET_EPILOGUE;
		epilogue->size = BLK_HDR_SIZE;
		
		//call sbrk to get more space
		free_block = mem_sbrk(new_size + BLK_HDR_SIZE);

		//if there is an error, return NULL
		if (free_block <= 0)
			return NULL ;
	
		//get the start of the freeblock
		free_block = (blockHdr *) ((char *) free_block - epilogue->size);

		//free_block size taking epilogue into account
		free_block->size = ((epilogue->size) + new_size) | 1;

		//now set the footer size of the newly created block
		blockFtr *ftr = (blockFtr *) ((char *) free_block - BLK_FTR_SIZE + ((free_block->size) & ~1));
		ftr->size = free_block->size;

		//adjust the epilogue
		epilogue = GET_EPILOGUE;
		epilogue->next_p = epilogue;
		epilogue->prior_p = epilogue;
		epilogue->size = BLK_HDR_SIZE | 1;

	} else{
		//otherwise, use the free block!
		//if there is too much space, split the space and put remainder in appropriate free list
		free_block = split_block(free_block->size, new_size, free_block);
		
		//use the space you now have
		free_block->size |= 1;
		blockFtr *ftr = (blockFtr *) ((char *) free_block - BLK_FTR_SIZE + ((free_block->size) & ~1));
		ftr->size |= 1; //set footer to allocated
		
		//remove the free block from the doubly linked list
		remove_from_free_lists(free_block);
	}
	return (void *) ((char *) free_block + BLK_HDR_SIZE);
}


			 


void remove_from_free_lists(blockHdr *head){
	//removes head from the free list which it belongs to
	//remove if it's free
	head->prior_p->next_p = head ->next_p;
	head->next_p->prior_p = head ->prior_p;
}






blockHdr *split_block(int old_size, int new_size, blockHdr *head) {
	//Assume new_size is already alligned

	//splits a block into a smaller block and adds the remaining to a free list
	//in order to split, the new_size 
	//minus the old_size must be greater than BLK_HDR_FTR_SIZE
	//using 40 as a threshold for minimum size of payload
	
	if((old_size - new_size) > (BLK_HDR_FTR_SIZE + 40)){

		//adjust the block's header and footer sizes
		head ->size = new_size;
		blockFtr *foot = (blockFtr *)((char *)head + new_size - BLK_FTR_SIZE);
		foot ->size = new_size;

		//now get the extra block
		blockHdr *extra_head = (blockHdr *) ((char *) foot + (BLK_FTR_SIZE));
		extra_head ->size = (old_size - new_size);

		//get the extra block's foot
		blockFtr *extra_block_foot = (blockFtr *)((char *)extra_head + extra_head ->size - BLK_FTR_SIZE);
		extra_block_foot ->size = extra_head ->size;


		//if the next block after extra_hand is free
		blockHdr *next_head = (blockHdr *)((char *)extra_block_foot + BLK_FTR_SIZE);
		if(!((next_head ->size)& (1))){
			//coalease the extra head
			coalease(extra_head);
		}

		
		//now add the extra block to it's appropriate free list
		add_to_free_lists(extra_head);
	}

	return head;
}



void *map_to_list(int size){

	//given a certain size, this function returns a pointer to it's corresponding free list
	
	blockHdr *list = (blockHdr *) mem_heap_lo();
	if(size < 290){
		return list;
	}else if (size < 540){
		return list + 1;
	}else if (size < 2080){
		return list + 2;
	}else{
		return list + 3;
	}
}




void *find_fit(size_t size){
	blockHdr *LAST_LIST = mem_heap_lo() + (NUM_OF_FREE_LISTS-1)*BLK_HDR_SIZE;
	//finds a free block within the set of free lists that matches the given size
	blockHdr* list = map_to_list(size);

	//now check all lists from that list to the last list
	//in order to find a suitable free block
	
	for(; list <= LAST_LIST; list = (blockHdr *)((char *)list + BLK_HDR_SIZE)){
		//for a given list, look to find if a free block greater than (or equal) the size seeking is there 
		blockHdr* curr_block = list ->next_p;
		for(;(curr_block != list); (curr_block = curr_block->next_p)){
			if((((curr_block ->size)) >= size) && !((curr_block ->size) & 1)){
				//if the size is larger than (or equal to) what we are looking for and it's free 
				//then found a free space
				return (void *)curr_block;
			}
			
		}
	}
	return NULL; //no free block found
}




void add_to_free_lists(blockHdr *head){
	//adds a block to the free list
	//check for boundary conditions
	if(head == NULL) return;

	//get list which the head belongs to
	blockHdr *free_list = map_to_list(head ->size);

	//now that we have the free list and the head, we simply add
	//the head immediately after the free list (it's next)
	
	head ->prior_p = free_list;
	head ->next_p = free_list->next_p;
	free_list->next_p->prior_p = head;
	free_list->next_p = head;
}



/*
 * mm_free - Freeing a block does nothing.
 */

/*void mm_free(void *ptr)
{
	//frees a block
	//in order to free a block, simply set it's size to not allocated and place it in a free list
	if(ptr == NULL) return;

	blockHdr *head = (blockHdr *)((char *)ptr - BLK_HDR_SIZE);
	blockFtr *foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head -> size) & ~1));
	
	//now set it to not allocated
	//free the block
	(head ->size) &= ~1;
	(foot ->size) &= ~1;
	
	//coalease and add to free list
	head = coalease(head);

	//add to appropriate free list
	add_to_free_lists(head);
}



blockHdr *coalease(blockHdr *head){
	//this function is used to combine the neighboring free blocks with the current free block
	//note that it removes the current head from any free block it resides in
	//must free last bit before coaleasing 
	
	if(((head->size)&1)) return head; //it is free - nothing to coalease

	//coalease if need
	blockFtr *foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head ->size) & ~1));

	blockFtr *prev_foot = (blockFtr *)((char *)head - BLK_FTR_SIZE);
	blockHdr *prev_head = (blockHdr *)((char *)head - ((prev_foot ->size)&(~1)));
	blockHdr *next_head = (blockHdr *)((char *)head + ((head->size)&(~1)));

	int new_size = 0;

	if(~((prev_foot ->size) & (1)) && ((next_head ->size)&(1))){
		//if the previous foot is free and the next head isnt
		//remove prev_head and the current head (already not in list) from the free lists they reside in so you can coalease them
		remove_from_free_lists(prev_head);

		//now actually coalease
		new_size = (prev_foot -> size) + (head -> size);
		head = (blockHdr *)((char *)head - (prev_foot ->size));
		head -> size = new_size;
		foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head ->size) & ~1));
		foot ->size = new_size;
	
	}else if(((prev_foot -> size) & (1)) && !((next_head ->size)&(1))){
		//if the previous food is not free then the next head is
		//remove next_head (not in list) and the current head (already not in list) from the free lists 
		remove_from_free_lists(next_head);

		//now actually coalease
		new_size = (head -> size) + (next_head -> size);
		//note that the head doesnt change
		head -> size = new_size;
		foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head -> size) & ~1));
		foot -> size = new_size;


	}else if(!((prev_foot -> size) & (1)) && !((next_head ->size)&(1))){
		//if the previous foot and the next head are both free
		//remove next_head, prev_head and the current head (already not in list) from the free lists
		remove_from_free_lists(next_head);
		remove_from_free_lists(prev_head);

		new_size = (head ->size) + (prev_foot -> size) + (next_head ->size);
		head = (blockHdr *)((char *)head - (prev_foot ->size));
		head ->size = new_size;
		foot = (blockFtr *)((char *)head -BLK_FTR_SIZE + ((head ->size) & ~1));
		foot ->size = new_size;
	}

	return head;
}*/



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){
	if (size == 0) {
		//if newsize is 0, call mm_free(ptr) and return NULL
		mm_free(ptr);
		return NULL;
	}else if (ptr == NULL){
		//if ptr is NULL, call and return mm_malloc(ptr)
		return mm_malloc(size);
	
	}else if(size > 0){
		//Otherwise: Return a block of size >= newsize that preserves 
		//all the data from the payload of the block ptr
		
		//this is the pointer to the head of the block to be realloced
		blockHdr *bp = (blockHdr *)((char *)ptr - BLK_HDR_SIZE);
		
		//the alligned size (actual size to check for)
		size_t new_size = ALIGN(size + BLK_HDR_FTR_SIZE);
	
		//get next block after bp
		blockHdr *next_block = (blockHdr *)((char *)bp + ((bp ->size) & ~1));
		size_t next_block_size = ((next_block ->size) & ~1);
		int next_block_free = !((next_block ->size) & ~1);
		size_t curr_block_size = ((bp ->size) & ~1);

		if(new_size <= curr_block_size){
			//case 1 - if newsize <= current size of ptr, just return ptr
			//if the new size is much smaller than the current size, then may have to split it
			return ptr;
		
		}else if(next_block_free && ((next_block_size + curr_block_size) >= new_size)){
			//case 2 - If the next block after ptr is free and its size 
			//plus the size of ptr is >= the requested newsize, remove the free
			//the size of ptr and return ptr
			
			//remove the next block from the free list
			remove_from_free_lists(next_block);

			//now change the sizes of the current block header and footer
			bp ->size &= ~1;
			bp ->size += next_block_size;
			blockFtr *foot = (blockFtr *)((char *)bp + ((bp->size) & (~1)) - BLK_FTR_SIZE);
			bp ->size |= 1;
			foot ->size = bp->size;

			//now return the resized block
			return (void *)((char *)bp + BLK_HDR_SIZE);
	
		}else if(next_block_free && is_last_block(next_block) ){
			//case 3 - If the next block after bp is free, but its size plus the
			//size of ptr is NOT >= the requested newsize, BUT this next
			//block is the 'last' block (before the epilogue), call
			//sbrk() to get additional size and rewrite the header and 
			//then the footer of bp to change the size of bp. return ptr. 

		       	//remove the next block from the free list
			remove_from_free_lists(next_block);

			//get additional size that should be allocated
			int add_size = new_size - (next_block_size + curr_block_size);
			
			//get current epilogue 
			blockHdr *epilogue = GET_EPILOGUE;
			epilogue -> size = BLK_HDR_SIZE;

			//now allocate this size by using sbrk
			blockHdr *free_block = mem_sbrk(add_size);
			
			//if there's an error during allocation return NULL
			if (free_block <= 0) return NULL ;

			//then set the header and footer sizes to reflect this change
			bp ->size = new_size|1;
			blockFtr *foot = (blockFtr *)((char *)bp + ((bp ->size) & (~1)) - BLK_FTR_SIZE);
			foot ->size = bp ->size;
			
			//adjust the epilogue
			epilogue = GET_EPILOGUE;
			epilogue->next_p = epilogue;
			epilogue->prior_p = epilogue;
			epilogue->size = BLK_HDR_SIZE | 1;

			//return newly extended block (actually payload)
			return (void *)((char *)bp + BLK_HDR_SIZE);
	
		}else if(is_last_block(bp)){
			//case 4 - if bp is the 'last' block (before the epilogue), call
			//sbrk to get additional size and rewrite the header and 
			//then the footer of bp to change the size of bp. return bp. 

			//get additional size that should be allocated
			int add_size = new_size - (curr_block_size);
			
			//get current epilogue
			blockHdr *epilogue = GET_EPILOGUE;
			epilogue->size = BLK_HDR_SIZE;
			
			//now allocated this size by using sbrk 
			blockHdr *free_block = mem_sbrk(add_size);
			
			//if there's an error during allocation return NULL
			if (free_block <= 0) return NULL;

			//then set the header and footer sizes to reflect this change
			bp ->size = new_size|1;
			blockFtr *foot = (blockFtr *)((char *)bp + ((bp ->size) & (~1)) - BLK_FTR_SIZE);
			foot ->size = bp ->size;
			
			//adjust the epilogue
			epilogue = GET_EPILOGUE;
			epilogue->next_p = epilogue;
			epilogue->prior_p = epilogue;
			epilogue->size = BLK_HDR_SIZE | 1;

			//return newly extended block (actually payload)
			return (void *)((char *)bp + BLK_HDR_SIZE);

		}else{
			//use malloc to get more space and copy the payload over to this space 
			
			//malloc with the new size
			void *new_blk = mm_malloc(size);

			//copy payload
			memcpy(new_blk,ptr,curr_block_size);
			
			//free previous block (ptr)
			mm_free(ptr);

			//return newly malloced block
			return new_blk;
		}
	}
	return NULL;
}

int is_last_block(blockHdr *head){
	//this method returns 1 if the current block is the last block just before the epilogue
	//otherwise, return 0
	
	blockHdr *epilogue = GET_EPILOGUE;
	if((blockHdr *)((char *)head + ((head ->size) & (~1)))>= epilogue) return 1;
	return 0;
}



void mm_free(void *ptf){

	//frees a block
	//in order to free a block, simply set it's size to not allocated and place it in a free list
	
	if(ptr == NULL) return;

	blockHdr *head = (blockHdr *)((char *)ptr - BLK_HDR_SIZE);
	blockFtr *foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head -> size) & ~1));

	//now set it to not allocated
	//free the block
	(head ->size) &= ~1;
	(food ->size_ &= ~1;

	 //coalease and add to free list
	 head = coalease(head);

	 //add to appropriate free list
	 add_to_free_lists(head);
}



blockHdr *coalease(blockHdr *head){
	
	//this function is used to combine the neighbouring free blocks with the current free block
	//It removes the current head from any free block it resides in
	
	if(((head->size)&1)) return head; //it isn't free. Nothing to coalease

	//coalease if there's anything to coalease
	blockFtr *foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head ->size) & ~1));

	//now we have the head and the foot
	blockFtr *prev_foot = (blockFtr *)((char *)head - BLK_FTR_SIZE);
	blockHdr *prev_head = (blockHdr *)((char *)head - ((prev_foot ->size)&(~1)));
	blockHdr *next_head = (blockHdr *)((char *)head + ((head->size)&(~1)));

	int new_size = 0;

	if(!((prev_foot ->size) & (1)) && ((next_head ->size)&(1))){
		//if the previous foot is free and the next head isn't

		//remove prev_head and the current head (already not in list) from the free lists they reside in to coalease them
		remove_from_free_lists(prev_head);

		//now actually coalease
		new_size = (prev_foot -> size) + (head -> size);
		head = (blockHdr *)((char *)head - (prev_foot ->size));
		head -> size = new_size;
		foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head ->size) & ~1));
		foot ->size = new_size;

	}else if(((prev_foot -> size) & (1)) && !((next_head ->size)&(1))){
		//if the previous foot is not free and the next head is

		//remove next_head (not in list) and the current head (already not in list) from the free lists they reside in
		remove_from_free_lists(next_head);

		//now actually coalease
		new_size = (head -> size) + (next_head -> size);
		//head doesn't change
		head -> size = new_size;
		foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head -> size) & ~1));
		foot -> size = new_size;

	}else if(!((prev_foot -> size) & (1)) && !((next_head ->size)&(1))){
		//if the previous foot and the next head are both free
		
		//remove next_head, prev_head and the current head (already not in list) from the free lists they reside in 
		//to coalease them

		remove_from_free_lists(next_head);
		remove_from_free_lists(prev_head);

		//now actually coalease
		new_size = (head ->size) + (prev_foot -> size) + (next_head ->size);
		head = (blockHdr *)((char *)head - (prev_foot ->size));
		head ->size = new_size;
		foot = (blockFtr *)((char *)head - BLK_FTR_SIZE + ((head ->size) & ~1));
		foot ->size = new_size;
	}

	return head; //return the new head
}




int mm_check()
	//heap consistency checker. Just two cases are checked
	//others are fairly straightfoward
	
	//check if all free blocks are actually free
	int k = NUM_OF_FREE_LISTS-1;
	blockHdr *list_start = (blockHdr *)mem_heap_lo();
	LAST_LIST = list_start + k;

	int error_f = 0;
	//check all the free blocks until you get to the start again
	
	for(;(list_start<=LAST_LIST)&& !error_f;list_start = list_start +1){
		blockHdr *curr_b = list_start->next_p;
		for(;(curr_b != list_start)&&!error_f;curr_b = curr_b ->next_p){
			if((curr_b->size &1)&&(curr_b != list_start)){
				//flag an error
				printf("A FREE BLOCK WAS NOT FREE. CHECK : % %p", curr_b);
				error_f = 1;
				break;
			}
		}
	}

	
	if(error_f >0) return error_f; //return a flag for error

	//check whether headers and footers actually have the same size
	
	blockHdr *alloc_start = (blockHdr *)((char *)LAST_LIST + BLK_HDR_FTR_SIZE);

	//check all blocks except the last one
		for(;(alloc_start <= (blockHdr *)mem_heap_hi());alloc_start = (blockHdr *)((char *)alloc_start + (alloc_start ->size))){
			blockFtr *blk_e = (blockFtr *)((char *)alloc_start + (alloc_start ->size) - BLK_FTR_SIZE);
			if((alloc_start ->size) != (blk_e ->size)){
				printf("HEADER AND FOOTER DO NOT HAVE THE SAME SIZE. CHECK : %p",alloc_start);
				break;
			}
		}
	return 0;
}


/*  void *oldptr = ptr;
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
}*/

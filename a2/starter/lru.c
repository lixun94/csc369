#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

static int count;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	
	int index = 0;
	int min = coremap[0].pte->ref;
	int i;
	
	// find the index of pte with smallest counter
	for( i = 0; i < memsize; i++){
			int curr = coremap[i].pte->ref;
			// set current least recently used
	        if( curr < min){
	        	min = curr;
	        	index = i;
	        }
		}

	return index;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {
	
	// increasement of counter
	count++;
	
	// update p -> stamp;
	(p->ref) = count;
	return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
	count = 0;
	return;
}

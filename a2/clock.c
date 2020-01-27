#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

static int clock_p;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
	
	while (1) {
		// check if ref = 0 as the clock_p pointed
		if ((coremap[clock_p].pte->ref) == 0) {
			// return clock_p as the evict one
			return clock_p;
		}
		// not referenced then set ref to 0
		coremap[clock_pointer].pte->ref = 0;
		// next clock_p
		clock_pointer = (clock_pointer + 1) % memsize;

	}
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {

	// if referenced, set ref to 1
	p->ref = 1;
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
	clock_pointer = 0;
}

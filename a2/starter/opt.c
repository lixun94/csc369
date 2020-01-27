#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"

#include "sim.h"
#define MAXLINE 256

extern int debug;

extern struct frame *coremap;

int *distances;

typedef struct node_t {
	// virtual address
    addr_t vaddr;  
    
    // pointer to next node
    struct node_t *next;    
} node;

node *root; 
node *cur; 

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {

	int i;
    // allocate space for distance array
    
	for (i = 0; i < memsize; i++) {
		int dist = 0;
		//virtual address
		addr_t vaddr = coremap[i].vaddr;
        // look for next access in tracefile, record distance
        node *temp = cur->next;
        while (temp && vaddr != temp->vaddr) { 
        	// traverse until vaddrs match
            dist++;
            temp = temp->next;
        }

        if (temp) { 
        	// found next access point
            distances[i] = dist;
        } else { 
        	// frame is never accessed again, evict optimal frame
            return i;
        }
    }
    
    // find max distance in distances list
    int max_frame = -1;
    int max_distance = -1;
    for (i = 0; i < memsize; i++) {
        // if distance is greater than max distance
        if (distances[i] > max_distance) {
            // update max distance and max frame
            max_distance = distances[i];
            max_frame = i;
        }
    }
    
    // evict frame with longest distance
    return max_frame;
	
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */

void opt_ref(pgtbl_entry_t *p) {

    cur = cur->next;
    if (!cur)
        cur = root;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
	
    root = NULL;
    cur = NULL;
    
    // allocate space for distance array
    distances = malloc(sizeof(int) * memsize);
    
    char buf[MAXLINE];
    char type;
    addr_t vaddr;
    
    // open tracefile for reading
    FILE *fp = fopen(tracefile, "r");
    while (fgets(buf, MAXLINE, fp) != NULL) {
        
        if (buf[0] == '=')
            continue;
        
        // read line
        sscanf(buf, "%c %lx", &type, &vaddr);
        
        // store vaddr in new node
        node *new_node = malloc(sizeof(node));
        new_node->vaddr = (vaddr >> PAGE_SHIFT) << PAGE_SHIFT;
        new_node->next = NULL;
        
        // add to trace list
        if (root) { 
        	// list not empty, add new node to tail
            cur->next = new_node;
        } else { 
        	// list empty, make new list using new node as head
            root = new_node;
        }
        
        // set current node to new node
        cur = new_node;
    }
    
    // when complete, set current node as head of list
    cur = root;

}


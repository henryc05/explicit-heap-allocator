// CS 107 - explicit.c
// Henry Chang
// This file houses the explicit allocator which is more efficient than implicit due to it's ability to coalesce and also rewire pointers through the free-list nodes (doubly linked list). This allocator makes use of the newNode struct to respectfully impelement the free-list nodes logic and banks on coalescing for better run times.

#include "./allocator.h"
#include "./debug_break.h"
#include <stdio.h>
#include <string.h>

#define BYTES_PER_LINE 32
#define MINIMUM_BYTES_16 16
#define MINIMUM_BYTES_24 24
#define HEADER_SIZE 8

typedef size_t header_t;

typedef struct freeNode{
    header_t* prev;
    header_t* next;
} freeNode;

static header_t* segment_start;
static size_t segment_size;
static header_t *end_heap;
static header_t* first_free_ptr = NULL;

static size_t count_free;
static size_t count_allocated; 

// checks if the header is free
bool is_free(header_t *header){
    return !(*(header) & 1);
}
// sets the header to the function and allocated
void set_header(header_t *header, size_t size, bool status){
    if (status){
        *(header) = size;
    }
    else {
        *(header) = (size | 1);
    }
}

// returns the payload size through bitwise 
size_t get_payload_size(header_t *header){
    return *(header) & ~1; 
}

// returns a pointer to the payload of the header through pointer arithmetic
void *header_to_payload(header_t *header){
    return (void*)((char*)header + HEADER_SIZE); 
}

//returns the pointer to the header through pointer arithmetic
header_t *payload_to_header(void *payload){
    return (header_t*)((char*)payload - HEADER_SIZE);
}

// returns a pointer to the next header through pointer arithmetic
header_t *next_header(header_t *current){
    return (header_t*)((char*)current + HEADER_SIZE + get_payload_size(current));
}

// rounds up a number to the inputted number "mult"
size_t roundup(size_t sz, size_t mult){
    return ((sz + mult - 1) & ~(mult - 1));
}

// bitwise performance to change the header to allocated. 
void change_to_allocated(header_t *header){
    *header |= 1;
}

//additional helper functions in explicit.c

// gets the free block in the free-list based on pointer arithmetic
freeNode* get_free_block(header_t* header){
    return (freeNode*)((char*)header + HEADER_SIZE);
}

// helper functions that checks if the list node is in bounds + also checks if the node is NULL or not
header_t* get_prev_free(header_t* current){
   freeNode* free_node = get_free_block(current);
    return free_node->prev; 
}

header_t* get_next_free(header_t* current){
    freeNode* free_node = get_free_block(current);
    return free_node->next;                                    
}

// helper functions that change the next or prev nodes in terms of rewiring nodes (used in delete and add)
void set_prev_free(header_t* current, header_t* previous){
    freeNode* free_node = get_free_block(current);
    free_node->prev = previous;
}

void set_next_free(header_t* current, header_t* next){
    freeNode* free_node  = get_free_block(current);
    free_node->next = next;
}

// removes the current node by rewiring the prev and next pointers to point each other and basically ignore the middle node
void remove_free_node(header_t* current_block){
    
    if (get_prev_free(current_block)){
        set_next_free(get_prev_free(current_block), get_next_free(current_block));
    }
    if (get_next_free(current_block)){
        set_prev_free(get_next_free(current_block), get_prev_free(current_block));
    }
    if (first_free_ptr == current_block){
        first_free_ptr = get_next_free(current_block);
    }
}

// adds a free node to the start of the free node list to optimize efficiency.
void add_free_node(header_t* current_block){
    set_prev_free(current_block, NULL);
    set_next_free(current_block, first_free_ptr);

    if (first_free_ptr != NULL){
        set_prev_free(first_free_ptr, current_block);
    }
    first_free_ptr = current_block; 
}

// myfree was within the heap and it was at the very start of the payload 
bool myinit(void *heap_start, size_t heap_size) {
    //first block and initial block size
    if (heap_size < MINIMUM_BYTES_24){
        return false;
    }
    segment_start = heap_start;
    segment_size = heap_size;
    // set the first free pointer to the payload of the entire memory
    
    end_heap = (header_t*)((char*)segment_start + segment_size);
    set_header(segment_start, segment_size - HEADER_SIZE, true);

    first_free_ptr = (header_t*)heap_start;
    set_next_free(first_free_ptr, NULL);
    set_prev_free(first_free_ptr, NULL);

    count_allocated = 0;
    count_free = 1; 

    return true;
}

// This split block function allows for mymalloc to handle edge cases where the next header has a payload 0 zero. If this was the case, mymalloc would call split block and made the next available header free.
void split_block(header_t *current_header, size_t rounded_size, size_t payload){
    set_header(current_header, rounded_size, false);
    header_t* new_header = next_header(current_header);
    set_header(new_header, (payload - rounded_size - HEADER_SIZE), true);
    add_free_node(new_header);
    count_free++;
    
}

// this function is used to merge blocks to the right in the heap that are allocated. This is for efficiency purposes in explicit
bool coalesce(header_t* currHeader){
    if (currHeader == NULL){
        return false;
    }
    header_t* nextHeader = next_header(currHeader);
    if ( (nextHeader < end_heap) && is_free(nextHeader)){ // nextHeader < end_heap
        size_t total = HEADER_SIZE + get_payload_size(currHeader) + get_payload_size(nextHeader);
        remove_free_node(nextHeader);
        count_free--;
        set_header(currHeader, total, false);
        return true;  
    }
    return false;
}

// This function allocates the size bytes of memory and returns a pointer to the allocated memory. 
void *mymalloc(size_t requested_size) {
    if (requested_size == 0){
        return NULL;
    }
    if (requested_size > MAX_REQUEST_SIZE){
        return NULL;
    }
    
    if (first_free_ptr == NULL){ // edge case added to check if the first pointer is not NULL
        return NULL;
    }
    
    size_t rounded_req_size = roundup(requested_size, HEADER_SIZE);
    if (rounded_req_size < 16){ // edge case to set the size to 16 if it's less than 16.
        rounded_req_size = 16;
    }
    header_t* start = first_free_ptr;
    
    while (start != NULL){ // looping through the free list and mallocing free blocks from there.
        size_t payload = get_payload_size(start);
        if ((payload >= rounded_req_size) && is_free(start)){
            change_to_allocated(start);
            count_allocated++;
            remove_free_node(start);
            count_free--;
            if (payload - rounded_req_size >= 24){ // should probs change to 16
                split_block(start, rounded_req_size, payload);
            }
            return header_to_payload(start);
        }
        start = get_next_free(start);
    }                                    
    return NULL;
}

void myfree(void *ptr) {
    if (!ptr){
        return; 
    }
    header_t* header = payload_to_header(ptr);
    *(header) &= ~1;
    add_free_node(header); // adding free node to the free list because we're freeing in this function
    coalesce(header);
    set_header(header, get_payload_size(header), true); // set it to a free payload/header after coalescing because we're inside myfree.
    // --- for validate_heap purposes ---
    count_allocated--;
    count_free++;
}

// This function changes the size of the memory block pointed to by old_ptr to new_size bytes. If the new size is larger than the old size, this realloc will memcpy into the heap. 
void *myrealloc(void *old_ptr, size_t new_size) {
    
    size_t rounded_req_size = roundup(new_size, HEADER_SIZE); // initialize the new_size but round up

    if (old_ptr == NULL){
        return mymalloc(new_size);
    }
    if (new_size == 0){
        myfree(old_ptr);
        return NULL;
    }
    header_t* old_header = payload_to_header(old_ptr);
    if (rounded_req_size < 16){
        rounded_req_size = 16;
    }
    size_t old_payload = get_payload_size(old_header);   
    bool keepCoalescing = true;

    // coalesce as much as possible!
    while (keepCoalescing){ 
        keepCoalescing = coalesce(old_header); 
    }
    old_payload = get_payload_size(old_header); // update payload
    
    if (rounded_req_size == old_payload){ // edge case where if the user reallocs to the same size
        return old_ptr;
    }   
    if (rounded_req_size < old_payload){     
        set_header(old_header, old_payload, false);
        if (rounded_req_size + MINIMUM_BYTES_24 <= get_payload_size(old_header) ) { // edge case of if there is less than or equal to old paylod size 
            split_block(old_header, rounded_req_size, get_payload_size(old_header));
        }
        return old_ptr; 
    }else{
        void *new_ptr = mymalloc(new_size);
        memcpy(new_ptr, old_ptr, (old_payload < new_size) ?  old_payload: new_size);
        myfree(old_ptr);
        return new_ptr; 
    }
}

// This function is implemented to help debug and make sure my allocator is working correctly. It runs through several checks and edge cases to make sure they pass rigorous cases.
bool validate_heap(){
    char *heap_end = (char *)segment_start + segment_size;
    header_t *current = segment_start;
    //header_t *start = first_free_ptr;

    size_t total_allocated = 0;
    size_t total_free = 0;

    while ((char*)current < heap_end){
        size_t payload = get_payload_size(current);

        if (payload < 8){
            printf("payload less than 8\n");
            breakpoint();
            return false;
        }
        if (payload % 8 != 0){
            printf("payload not mult of 8\n");
            breakpoint();
            return false;
        }
        if (is_free(current)){
            total_free++;
        } else{
            total_allocated++;
        }
        current = next_header(current);      
    }
    
    if ((total_free != count_free) || (total_allocated != count_allocated)){
        printf("validate_heap counted %zu free blocks, while internal count is %zu blocks\n", total_free, count_free);
        printf("validate_heap counted %zu allocated blocks, while internal count is %zu blocks\n", total_allocated, count_allocated);
    }
    if ((char*)current != heap_end){
        printf("Heap extends beyond segment end!\n");
        breakpoint();
        return false; 
    } 
    return true;
}


/* Function: dump_heap
 * -------------------
 * This function prints out the the block contents of the heap.  It is not
 * called anywhere, but is a useful helper function to call from gdb when
 * tracing through programs.  It prints out the total range of the heap, and
 * information about each block within it.
 */

void dump_heap() {
    printf("Heap segment starts at address %p, ends at %p\n", segment_start, end_heap);
    header_t* cur = segment_start;

    while((char*)cur < (char*)end_heap){
        printf("Header: %p. Payload Size: %zu. Status: %s\n", cur, get_payload_size(cur), is_free(cur) ? "F" : "A");
        cur = next_header(cur);
    }
}

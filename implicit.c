// CS 107 - implicit.c
// Henry Chang
// This file houses the implicit allocator which implements the first fit logic and improves memory utilization (compared to bump allocator). This function stores the block size as the payload size and traverses the heap through pointer arithemtic, heavily relying on helper functions. 

#include "./allocator.h"
#include "./debug_break.h"
#include <stdio.h>
#include <string.h>

#define BYTES_PER_LINE 32
#define MINIMUM_BYTES 16
#define HEADER_SIZE 8

typedef size_t header_t;

static header_t *segment_start;
static size_t segment_size;
static header_t *end_heap;

static size_t count_free; 
static size_t count_allocated;


// HELPER FUNCTION FOR HEADERS -------------------------------
// 1 is allocated 
// 0 is free
bool is_free(header_t *header){
    return !(*(header) & 1);
}

// sets the header to the function and allocated 
void set_header(header_t *header, size_t size, bool status){ 
    if (status){
        *(header) = size; //free
    }
    else{
        *(header) = (size + 1); //alloc
    }
}

// obtains the payload size of the header
size_t get_payload_size(header_t *header){ // DONE
    return *(header) & ~1;
}
// HELPER FUNCTION FOR BLOCKS -------------------------------
void *header_to_payload(header_t *header){ //
    return (void*)((char*)header + HEADER_SIZE);
}
header_t *payload_to_header(void *payload){ // looks good 
    return (header_t*)((char*)payload - HEADER_SIZE);
}
header_t *next_header(header_t *current){ // This is fine
    //printf("inside next_header\n");
    return (header_t*)((char*)current + HEADER_SIZE + get_payload_size(current));
}
// HELPER FUNCTION MISC -----------------------------------------
size_t roundup(size_t sz, size_t mult){
    return ((sz + mult - 1) & ~(mult - 1));
}

// changes the header to allocated 
void change_to_allocated(header_t *header){
    *header |= 1;
}

// this function is used by the test harness to make sure each allocator call is clean and starts fresh. 
bool myinit(void *heap_start, size_t heap_size) {
    if (heap_size < MINIMUM_BYTES){ 
        return false;
    }
    segment_start = heap_start;
    segment_size = heap_size;

    // pointer notation to get to the end of the heap
    end_heap = (header_t*)((char*)segment_start + segment_size);
    // sets the header of the function. 
    set_header(segment_start, segment_size - HEADER_SIZE, true);

    count_allocated = 0;
    count_free = 1;
    
    return true;
}

// helper function implemented to split the block itself. This function
// sets the header to allocated and moves to the next available header and sets that free. 
void split_block(header_t *current_header, size_t rounded_size, size_t payload){
    set_header(current_header, rounded_size, false);
    header_t* new_header = next_header(current_header); 
    set_header(new_header, (payload - rounded_size - HEADER_SIZE), true);
    count_free++;
}

// This function allocates the size bytes of memory and returns a pointer to the allocated memory. 
void *mymalloc(size_t requested_size) {
    if (requested_size == 0) { // edge case for when the requested size is zero
        return NULL;
    }
    if (requested_size > MAX_REQUEST_SIZE){ // edge case when the requested size exceeds the max limit
        return NULL;
    }
    size_t rounded_req_size = roundup(requested_size, HEADER_SIZE);
    header_t* start = segment_start; 

    // loops through the heap and finds the first fit for the block to be allocated.
    while ((char*)start < (char*)end_heap){      
        size_t payload = get_payload_size(start);      
        if ((payload >= rounded_req_size) && is_free(start)){
            change_to_allocated(start);
            count_allocated++;
            count_free--;
            if (payload - rounded_req_size >= MINIMUM_BYTES){
                split_block(start, rounded_req_size, payload);
            }
            return header_to_payload(start);
        }
        start = next_header(start);      
    }   
    return NULL;
}

// This function frees the pointer passed in.
void myfree(void *ptr) {
    if (!ptr){
        return;
    }
    header_t* header = payload_to_header(ptr);
    *(header) &= ~1; // bitwise to change the allocation to free
    count_allocated--;
    count_free++;
}


// This function changes the size of the memory block pointed to by the old_ptr to the new_size bytes. If the new size is larger than the old size, this realloc will memcpy into the heap. 
void *myrealloc(void *old_ptr, size_t new_size) {   
    if (old_ptr == NULL){ // edge case where if the old pointer is not viable, we would just malloc for the user
        return mymalloc(new_size);
    }
    if (new_size == 0){ // if the payload size is zero, we would just free the block
        myfree(old_ptr);
        return NULL;
    }
    // copied over realloc function from bump.c 
    void *new_ptr = mymalloc(new_size);
    memcpy(new_ptr, old_ptr, new_size);
    myfree(old_ptr);
    return new_ptr;
}

// This function is implemented to help debug and make sure my heap is working correctly. It runs through several checks and edge cases to make sure they pass rigorous cases.
bool validate_heap() {
    char *heap_end = (char *)segment_start + segment_size;
    header_t *current = segment_start;
   
    size_t total_allocated = 0;  
    size_t total_free = 0; 
    
    while ((char*)current < heap_end){
        size_t payload = get_payload_size(current);

        if (payload < 8){ // edge case
            printf("payload less than 8\n");
            breakpoint();
            return false;
        }
        if (payload % 8 != 0){ // edge case
            printf("payload not mult of 8\n");
            breakpoint();
            return false;
        }
        if (is_free(current)) {
            total_free++;            
        } else{
            total_allocated++;
        }
        current = next_header(current);
    }

  
    if ((char*)current != heap_end) {
        printf("Heap extends beyond segment end!\n");
        breakpoint();
        return false;
    }
    if ((total_free != count_free) || (total_allocated != count_allocated)){
        printf("validate_heap counted %zu free blocks, while internal count is %zu blocks\n", total_free, count_free);
        printf("validate_heap counted %zu allocated blocks, while internal count is %zu blocks\n", total_allocated, count_allocated);
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
void dump_heap() { // DONE 

    printf("Heap segment starts at address %p, ends at %p\n", segment_start, end_heap);
    header_t* cur = segment_start;

    while ((char*)cur < (char*)end_heap) {
        printf("Header: %p. Payload Size: %zu. Status: %s\n", cur, get_payload_size(cur), is_free(cur) ? "F" : "A");
        cur = next_header(cur);
    }
}

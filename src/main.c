#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/unistd.h>


/* Variable Declarations, And Globals
// ============================================================================ */

typedef uintptr_t ptr;

typedef struct memory_info_t {
    size_t size;
    struct memory_info_t* next;
    bool freed;
    bool being_used;
} MemoryInfo;

static ptr stack_bottom;
static MemoryInfo* memory_list = NULL; /* tail */
static MemoryInfo* memory_list_head = NULL; /* head */

/* Better C
// ============================================================================ */

#define _foreach(item, list) for (__typeof__(list) item =  list; item != NULL; item = item->next)
#define foreach(item_in_list) _foreach(item_in_list)
#define in ,
#define and &&
#define or ||
#define unless(pred) if (!(pred))
#define OUT_OF_MEMORY (void*)-1

/* Utilities
// ============================================================================ */

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define log(type, M, ...) fprintf(stderr, "[" type "] (%s: %d) " M "\n", __FILENAME__, __LINE__, ##__VA_ARGS__)
#define check(A, M, ...) if(!(A)) { log("ERROR", M, ##__VA_ARGS__); exit(1); }

ptr get_top_of_stack()
{
    void* dummy;
    ptr address = &dummy;
    return address;
}

/* Memory Management / Malloc Implementation
// ============================================================================ */

void add_to_memory_list(MemoryInfo *mem_info)
{
    /* advance list or set the tail for the first call */
    memory_list ? (memory_list_head->next = mem_info) : (memory_list = mem_info);
    memory_list_head = mem_info;
}

/* Returns a previously freed block, newly allocated space, or returns null if no memory is available. */
MemoryInfo* request_memory(size_t requested_size)
{
    /* try to find a free block in our previously requested memory list */
    foreach (block in memory_list) {
        if (block->freed and block->size >= requested_size) return block;
    }

    /* there was no previously allocated memory available, so we allocate some */
    MemoryInfo* memory = sbrk(sizeof(MemoryInfo) + requested_size); /* extra space for meta data */
    unless (memory == OUT_OF_MEMORY) {
        memory->size       = requested_size;
        memory->next       = NULL;
        memory->being_used = false;
        memory->freed      = false;
        add_to_memory_list(memory); /* keep track of this new chunk */
        return memory;
    }

    return NULL; /* no memory available what-so-ever! */
}

/* No more need for a free method! */
void* gc_malloc(size_t size_in_bytes)
{
    if (size_in_bytes <= 0) return NULL; /* silliness */

    MemoryInfo* mem_info = request_memory(size_in_bytes);
    return mem_info + 1; /* user doesn't need our meta-data */
}


/* Garbage Collector Implementation
// ============================================================================ */

/* returns true if the address 'ref' is in 'item' */
bool ref_points_to_item(void* ref, MemoryInfo* item)
{
    return (*((ptr*)ref) >= (item + 1) and *((ptr*)ref) < (void*)(item + 1) + item->size);
}

/* Reports all of the memory still marked as 'being used' */
size_t gc_memory_in_use()
{
    size_t sum = 0;
    foreach (block in memory_list) {
        unless (block->freed) sum += block->size;
    }

    return sum;
}

/* Used to check the stack and heap for references to addresses being used by our heap */
void mark_region(ptr start, ptr end)
{
    for (void* ref = start; ref < end; ref++) {
        foreach (item in memory_list) {
            if (ref_points_to_item(ref, item)) {
                item->being_used = true; /* our 'item' (= *ref) was referenced by 'block' at 'ref' */
            }
        }
    }
}

/* Cross references each block's internal references against each allocated block in our "heap" */
void mark_our_heap() {
    foreach (block in memory_list) {
        for (void* ref = (block + 1); ref < (void*)(block + 1) + block->size; ref++) {
            foreach (item in memory_list) {
                if (ref_points_to_item(ref, item)) {
                    item->being_used = true; /* our 'item' (= *ref) was referenced by 'block' at 'ref' */
                }
            }
        }
    }
}

/* Determine whether objects in our heap can still be accessed by the user - marking them if they are */
void mark_all()
{
    extern char end, etext; /* provided by the linker. end = end of BSS segment. etext = end of text segment */
    mark_region(&etext, &end); /* marks the BSS and initialized data segments */

    mark_region(get_top_of_stack(), stack_bottom);

    mark_our_heap();
}

/* Set all of the marked blocks free! */
void sweep()
{
    foreach (block in memory_list) {
        unless (block->being_used) block->freed = true;
        block->being_used = false; /* reset for next collection */
    }
}

/* Applies a 'mark and sweep' system where referenced objects marked as active will not be freed. */
void gc_collect()
{
    mark_all();
    sweep();
}

__attribute__((constructor))
void gc_init()
{
    stack_bottom = get_top_of_stack() + 256; /* well before the main method */
}

/* Garbage Collector Testing
// ============================================================================ */

void gc_test_reuse()
{
    printf("BEGINNING REUSE TEST\n");
    int* a = gc_malloc(sizeof(char));
    gc_collect(); /* should not delete a */
    int* b = gc_malloc(sizeof(char));
    check(a != b, "overwrote memory that was still being used");
    printf("FINISHED REUSE TEST\n");
}

void gc_test_stack_garbage()
{
    char* a = gc_malloc(1); /* garbage */

    size_t mem = gc_memory_in_use();
    check(mem == 1, "GC failed to initialize list memory, got %zu", mem);
    gc_collect();
    check(gc_memory_in_use() == 1, "GC failed to find a reference to 'a' [%llu] on the stack", &a);
}

void gc_test_stack()
{
    printf("BEGINNING STACK TEST\n");
    check(gc_memory_in_use() == 0, "GC initialization error");

    gc_test_stack_garbage();
    gc_collect();

    check(gc_memory_in_use() == 1, "GC failed to perform stack deallocation");
    printf("FINISHED STACK TEST\n");
}

void gc_test_heap()
{
    printf("BEGINNING HEAP TEST\n");
    {
        check(gc_memory_in_use() == 0, "GC initialization error");
        void *test = gc_malloc(1);
        void *reference = test;
        test = 0;
        check(gc_memory_in_use() == 1, "GC failed allocation");
        gc_collect();
        check(gc_memory_in_use() == 1, "GC made inappropriate de-allocation");
    }
    gc_collect();
    check(gc_memory_in_use() == 0, "GC failed to perform heap deallocation");
    printf("FINISHED HEAP TEST\n");
}

/* Main
// ============================================================================ */

int main()
{
    gc_test_stack();
    return EXIT_SUCCESS;
}
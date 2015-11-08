#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <stdbool.h>
#include <string.h>


/* Variable Declarations, And Globals
// ============================================================================ */

typedef u_int32_t uint32;

typedef struct memory_info_t {
    size_t size;
    struct memory_info_t* next;
    bool freed;
    bool is_active;
} MemoryInfo;

static uint32 stack_bottom = NULL;
static MemoryInfo* memory_list = NULL; /* tail */
static MemoryInfo* memory_list_head = NULL; /* head */


/* Utilities
// ============================================================================ */

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define log(M, ...) fprintf(stderr, "[ASSERT] (%s: %d) " M "\n", __FILENAME__, __LINE__, ##__VA_ARGS__)
#define check(A, M, ...) if(!(A)) { log(M, ##__VA_ARGS__); }

#define _foreach(item, list) for (__typeof__(list) item =  list; item != NULL; item = item->next)
#define foreach(item_in_list) _foreach(item_in_list)
#define in ,
#define and &&
#define or ||

void die(const char* message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

/* ebp is the stack frame of the method, perfect for getting the top one. */
#define get_top_of_stack(stack_top) __asm volatile ("movl %%ebp, %k0" : "=r" (stack_top))

/*
 * The bottom of the stack is stored in /proc/self/stat on most linux variants and cygwin.
 * This is not a very portable way of doing it, but for security reasons the stack is usually
 * randomized for security measures.
 */
uint32 get_bottom_of_stack()
{
    FILE* proc_stats_file = fopen("/proc/self/stat", "r");

    // see http://man7.org/linux/man-pages/man5/proc.5.html starting at /proc/[pid]/stat
    unsigned long stack_bottom_address;
    fscanf(proc_stats_file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u "
                            "%*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld "
                            "%*ld %*ld %*ld %*ld %*llu %*lu %*ld "
                            "%*lu %*lu %*lu %lu", &stack_bottom_address);

    fclose(proc_stats_file);
    return (uint32)stack_bottom_address;
}


/* Memory Management / Malloc Implementation
// ============================================================================ */

void add_to_memory_list(MemoryInfo *mem_info)
{
    /* add onto the linked list */
    if (memory_list_head == NULL) {
        memory_list = mem_info; /* first call, set the tail */
    } else {
        memory_list_head->next = mem_info; /* advance the list */
    }

    memory_list_head = mem_info;
}

/*
 * Returns a previously freed block, newly allocated space, or returns null if no memory is available.
 */
MemoryInfo* find_free_block(size_t requested_size)
{
    /* try to find a free block in our previously requested memory list */
    foreach (block in memory_list)
    {
        if (block->freed and block->size >= requested_size)
        {
            return block; /* found a usable block! */
        }
    }

    /* no previously allocated memory available, allocate some */
    MemoryInfo* memory = sbrk(sizeof(MemoryInfo) + requested_size); /* extra space for meta data */

    /* if the request didn't fail give it to the user! */
    if (memory != (void*)-1)
    {
        memory->size = requested_size;
        memory->next = NULL;
        memory->is_active = false; /* will be changed to true during marking process if it is 'in use' */
        memory->freed = false;
        add_to_memory_list(memory); /* keep track of this new chunk */
        return memory;
    }

    return NULL; /* no memory available what-so-ever! */
}

/*
 * No more need for a free method!
 */
void* gc_malloc(size_t size_in_bytes)
{
    if (stack_bottom == NULL) stack_bottom = get_bottom_of_stack(); /* first time */
    if (size_in_bytes <= 0) return NULL; /* silliness */

    /* get or allocate some memory from our linked list of memory */
    MemoryInfo* mem_info = find_free_block(size_in_bytes);

    return mem_info + 1; /* user doesn't need our meta-data */
}


/* Garbage Collector Implementation
// ============================================================================ */

/* reports all of the memory still marked as 'being used' */
size_t gc_memory_in_use()
{
    size_t sum = 0;
    foreach (block in memory_list) {
        if (!block->freed) sum += block->size;
    }
    return sum;
}

/* Used to check the stack and heap for references to addresses being used by our heap */
void mark_region(uint32 *start, uint32 *end)
{
    for (uint32 ref = *start; start < end; ref = start++) {
        foreach (item in memory_list) {
            if (ref >= (item + 1) and ref < (uint*)(item + 1) + item->size) {
                item->is_active = true; /* our 'item' was referenced by stack/bss (ref) at '*ref'*/
            }
        }
    }
}

/* Cross references each block's internal references against each allocated block in our "heap" */
void mark_our_heap() {
    /* look at each block used on the heap */
    foreach (block in memory_list) {
        /* checking each internal reference of a block */
        for (uint32* ref = (block + 1); ref < (uint32) (block + 1) + block->size; ref++) {
            /* to see if any of them refer to another item on the heap */
            foreach (item in memory_list) {
                if ((*ref >= (item + 1) and *ref < (uint32)(item + 1) + item->size)) {
                    item->is_active = true; /* our 'item' was referenced by 'block' at 'ref' */
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

    uint32 stack_top; get_top_of_stack(stack_top);
    mark_region(stack_top, stack_bottom);

    mark_our_heap();
}

/* Set all of the marked blocks free! */
void sweep()
{
    foreach (block in memory_list) {
        if (!block->is_active) {
            block->freed = true;
        }

        // printf("block %u, active = %i, freed = %i, size = %i\n", block + 1, block->is_active, block->freed, block->size);
        block->is_active = false; /* reset marking for the next collection */
    }
}

/* Applies a mark and sweep system where referenced objects will be marked as active/will not be freed. */
void gc_collect()
{
    if (memory_list_head == NULL) die("Initialize the garbage collector first!");

    mark_all();
    sweep();
}

/* Garbage Collector Testing
// ============================================================================ */

void gc_test_reuse()
{
    printf("BEGINNING REUSE TEST\n");
    int* a = gc_malloc(sizeof(char));
    gc_collect();
    int* b = gc_malloc(sizeof(char));
    check(a != b, "overwrote memory still being used");
    printf("FINISHED REUSE TEST\n");
}

void gc_test_stack()
{
    printf("BEGINNING STACK TEST\n");
    for (int i = 0; i < 1; i ++) {
        {
            check(gc_memory_in_use() == 0, "GC initialization error");
            char* a = gc_malloc(1);
            check(gc_memory_in_use() == 1, "GC failed allocation");
            gc_collect();
            check(gc_memory_in_use() == 1, "GC made inappropriate de-allocation");
        }
        gc_collect();
        check(gc_memory_in_use() == 0, "GC failed to perform stack deallocation");
    }
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

void gc_test_hard()
{
    printf("BEGINNING HARD TEST\n");

    typedef struct node_t { struct node_t* next; int val; } Node;
    Node* a = gc_malloc(sizeof(Node));
    a->val = 1;
    a->next = NULL;

    Node* b = gc_malloc(sizeof(Node));
    b->val = 2;
    b->next = a;

    Node* c = gc_malloc(sizeof(Node));
    c->val = 1;
    c->next = b;

    {
        check(gc_memory_in_use() == sizeof(Node) * 3, "Nodes not properly allocated");
        gc_collect();
        check(gc_memory_in_use() == sizeof(Node) * 3, "size is off");
        printf("leaked %u\n", gc_malloc(50));
        check(gc_memory_in_use() == (sizeof(Node) * 3) + 50, "Additional allocation failed");
    }

    gc_collect();
    check(gc_memory_in_use() == (sizeof(Node) * 3), "size is off");

    a = NULL;
    b = NULL;
    c->next = NULL;
    gc_collect();
    check(gc_memory_in_use() == (sizeof(Node)), "Only one node should be accessible still");

    c = NULL;
    gc_collect();
    check(gc_memory_in_use() == 0, "No nodes are accessible and should be deleted");
    printf("FINISHED HARD TEST\n");
}

int main()
{
    gc_test_reuse();
    return EXIT_SUCCESS;
}
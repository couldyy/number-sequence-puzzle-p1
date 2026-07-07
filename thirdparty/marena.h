#ifndef MARENA_H
#define MARENA_H

// TODO: get rid of all includes, and include them only when some macro is defined
#include <assert.h>     // assert()
#include <stdint.h>     
#include <stdlib.h>     // malloc()
#include <string.h>     // memcpy()

/*
 TODO:
    [x] store pointer to end in Arena struct and DONT iterate over Pages, just allocate at last one
    [x] create functions for resetting arena (mark all pages as free)
    [ ] create context_* funcions if there are corresponding #define (context_alloc(), context_reset(), context_free())
        which will allow to call all functions without providing Arena address
    [ ] implement different backends (single macro and different unerlying functions for it)
    [ ] store and restore functions for arena memory
    [ ] handle arena_alloc(&a, 0) (size = 0)
    [ ] implement with some debug compile flage, some debug info:
        - how many allocations were called on arena
        - how mant bytes allocated (in arena and Page)
        - how many times new page was allocated
        - how many bytes lost (fragmentation: (mb calculate avg lost per allocation)
                    Page size: 8192 [******|               ] -> [******************************|   ]
                            1000 bytes ^^                               ^^^^^
                                            7500 requested - dont fit, new page has to be allocated, 7192 bytes lost

        - how many times allocation exceed 'page_size'
    [ ]? create algorithm, so that if allocation frequently causes new page allocation - increase page size?

    [ ]? Rename Page to Region?
 */


//#define MARENA_DEBUG

#ifdef MARENA_DEBUG
#define MARENA_INIT_VALUE 0xcd
#else
#define MARENA_INIT_VALUE 0
#endif

#define MARENA_STR_PREFIX "marena"

#ifndef MARENA_DEFAULT_PAGE_SIZE 
#define MARENA_DEFAULT_PAGE_SIZE 8196
#endif

// should be enough for most systems. You can manually redefine it, if this alignment doesnt work for you
#ifndef MARENA_ALIGNMENT 
#define MARENA_ALIGNMENT (sizeof(void*))
#endif

#define MARENA_ALIGN(sz) ((sz + MARENA_ALIGNMENT - 1) & ~(MARENA_ALIGNMENT - 1))

#define MARENA_INTERNAL_ZEROED 0x1

// if MARENA_STATIC_RETURN_NULL_ON_FULL, and (flags & MARENA_O_ARENA_STATIC), then
// allocator will return NULL if there are no free space for allocation, otherwise it will print 
// error message and exit(1)
#define MARENA_O_ARENA_DYNAMIC 0
#define MARENA_O_ARENA_STATIC 1
#define MARENA_O_DYNAMIC_PAGE_SIZE 2

#ifndef MARENA_DYNAMIC_PAGE_SIZE_GROW_FACTOR 
#define MARENA_DYNAMIC_PAGE_SIZE_GROW_FACTOR 2
#endif

#ifndef MARENA_PAGEMISS_FACTOR 
#define MARENA_PAGEMISS_FACTOR 0.005f
#endif

//#define PAGEMISS_PER_ALLOC(arena) ((float)(arena)->pagemiss_cnt_local / (float)(arena)->allocs_cnt_local)
#define MARENA_PAGEMISS_PER_ALLOC(arena) ((float)(arena)->_pagemiss_cnt / (float)(arena)->_allocs_cnt)   // global counters

// pointer to that struct is actual start of page, usable memory is at &page + sizeof(Page)
//  'start_free' contains start address of free memory in that page
typedef struct Page {
    void* start_free;   // pointer (offset from the begining) to the start of allocatable memory in page
    size_t allocated;    // bytes
    size_t size;        // bytes

    struct Page* next;
    //int is_zeroed;    // TODO: implement that field and check for it in arena_*_zero() functions, 
                        // to reduce memset() functions
} Page;

typedef struct {
    Page* start;
    Page* end;
    size_t page_size;       // size for further allocations of page
    int flags;
    uint64_t _pagemiss_cnt;
    uint64_t _allocs_cnt;
#ifdef MARENA_DEBUG
    uint64_t _page_size_grows_cnt;
#endif
} Arena;


typedef struct {
    Arena* mark_for;    // to prevent later restoring from mark of different arena
    Arena arena;
    Page last_page;
} Arena_mark;


// initializes page
Page* init_page(size_t size);

// initializes arena on the heap
Arena* arena_init_heap(size_t size);

// allocs with internal flags
void* arena__alloc_flag(Arena* arena, size_t size, int flags);

// allocates at least 'size' aligned bytes within 'arena' memory
#define arena_alloc(arena, size) arena__alloc_flag(arena, size, 0)

// TODO: performance is 10x worse then in arena_alloc(), optimise it
// allocates at least 'size' aligned bytes within 'arena' memory and sets them to 0 (If MARENA_INIT_VALUE is set so)
#define arena_alloc_zero(arena, size) arena__alloc_flag(arena, size, MARENA_INTERNAL_ZEROED)



// Deallocates all pages in arena
// Note: does NOT deallocate 'arena' struct itself
void arena_free(Arena* arena);


// Marks all pages as free
#define arena_reset(arena) arena__reset_flag(arena, 0)

// Marks all pages as free, and sets them to 0
#define arena_reset_zero(arena) arena__reset_flag(arena, MARENA_INTERNAL_ZEROED)

// reset WITH flag
void arena__reset_flag(Arena* arena, int flags);

#define arena_restore(arena, mark) arena_restore_flag(arena, mark, 0)
#define arena_restore_zero(arena, mark) arena_restore_flag(arena, mark, MARENA_INTERNAL_ZEROED)

void arena_restore_flag(Arena* arena, Arena_mark* mark, int flags);
Arena_mark* arena_mark_on_arena(Arena* arena);

#endif // MARENA_H


#ifdef MARENA_IMPLEMENTATION

#define arena__reset_page_flag(page, flags) \
    do { \
        if (flags & MARENA_INTERNAL_ZEROED) { \
            /* FIXME: doesnt work for some reason */ \
            /*memset((char*)page + sizeof(Page), MARENA_INIT_VALUE, page->allocated);  */ /* memset only allocated bytes */ \
            memset((char*)page + sizeof(Page), MARENA_INIT_VALUE, page->size);  /* TODO: test this, does performance and quality differ? */ \
        } \
 \
        page->allocated = 0; \
        page->start_free = page + sizeof(Page);     /* reset free ptr to begining */ \
    } while (0)

// allocates requested size + sizeof(Page) strcture, and writes that structure in the beginnings
Page* init_page(size_t size)
{
    // (by 'page' means OS memory page, NOT marena's 'Page' struct
    // TODO: 'allocated' and 'size' are misleading beacuse they dont show allocated 'Page' struct
    //       in addition, if user requested page-aligned size of memory, " + sizeof(Page)" may cause for aligning to next page size,
    //      meaning leaking memory
    assert(size > 0);
    Page* page = malloc(sizeof(Page) + size);
    assert(page != NULL && "malloc failed at init page");
    void* usable_mem_start = (char*)page + sizeof(Page);  // offset actual start of usable memory TODO: what if user does smth nasty with metadata?
    page->start_free = usable_mem_start;
    page->allocated = 0;
    page->size = size;
    page->next = NULL;
    return page;
}

void* arena_realloc(Arena* arena, void* old_ptr, size_t old_size, size_t new_size)
{
    assert(arena != NULL);

    // TODO: would be nice to also change metadata of the page of requested 'old_ptr' but it will require a lot of time to find that page
    if (new_size <= old_size) {
        return old_ptr;
    }
    // quick way of realloc, implement more optimal later
    void* new_ptr = arena__alloc_flag(arena, new_size, 0);
    memcpy(new_ptr, old_ptr, old_size);
    return new_ptr;
}
// alloc with internal flags
void* arena__alloc_flag(Arena* arena, size_t size, int flags)
{
    assert(arena != NULL);
    size_t size_aligned = MARENA_ALIGN(size);

    Page* page;
    Page* prev_page;

    // init is a special case, no flags are checked, since in all of them at least 1 page must be allocated
    // also first page allocation doesnt count as page miss
    if (arena->start == NULL) {
        if (arena->page_size <= 0) { arena->page_size = MARENA_DEFAULT_PAGE_SIZE; }
        page = init_page(size_aligned > arena->page_size ? size_aligned : arena->page_size);    // TODO: if requested size > page_size, multiply by some factor?
        arena->start = page;
        arena->end = page;
        goto alloc;
    }

    page = arena->end;
    prev_page = NULL;
    while (page != NULL && page->allocated + size_aligned > page->size) {
        prev_page = page;
        page = page->next;
    }
    // just allocate new page, if there are no free space left in pages
    if (page == NULL) {
        if (arena->flags & MARENA_O_ARENA_STATIC) {
        #ifdef MARENA_STATIC_RETURN_NULL_ON_FULL
            return NULL;
        #else
            fprintf(stderr, MARENA_STR_PREFIX": Not enough memory in the arena for requested size (Arena set to STATIC)\n");
            exit(1);
        #endif 

        }

        arena->_pagemiss_cnt += 1;
        // grow page_size if allocations causes new page allocs frequently
        if ((arena->flags & MARENA_O_DYNAMIC_PAGE_SIZE) && (MARENA_PAGEMISS_PER_ALLOC(arena) >= MARENA_PAGEMISS_FACTOR)) {
            arena->page_size *= MARENA_DYNAMIC_PAGE_SIZE_GROW_FACTOR;  // TODO multiplication factor?

        #ifdef MARENA_DEBUG
            arena->_page_size_grows_cnt++;
        #endif

        }
        size_t page_alloc_size = (size_aligned > arena->page_size) ? (size_aligned) : arena->page_size;    // TODO: if requested size > page_size, multiply by some factor?
                                                                                                           // TODO: update page_size if size > page_size ?
        page = init_page(page_alloc_size);
        if (prev_page != NULL) {
            prev_page->next = page; 
        }
        //arena->end = page;
    }

alloc:

    // after reset there may be some pages already allocated and linked, update in that case
    if (arena->end != page) { arena->end = page; }
    arena->_allocs_cnt++;
    page->allocated += size_aligned;
    void* ret = page->start_free;
    if (flags & MARENA_INTERNAL_ZEROED) {
        memset(ret, MARENA_INIT_VALUE, size_aligned);
    }
    page->start_free = (char*)page->start_free + size_aligned;
    return ret;
}


void arena_free(Arena* arena) 
{
    assert(arena != NULL);
    Page* current_page = arena->start;
    Page* next = NULL;
    while (current_page != NULL) {
        next = current_page->next;
        free(current_page);
        current_page = next;
    }
    arena->start = NULL;
    arena->end = NULL;
} 

void arena__reset_flag(Arena* arena, int flags)
{
    assert(arena != NULL);
    for (Page* page = arena->start; page != NULL; page = page->next) {
        arena__reset_page_flag(page, flags);
    }
    arena->end = arena->start;
}


// TODO: creating mark on an arena is inconveniet, since they deallocates automatially after restore, create them on stack instead
// OR keep that function, just mark them as creating on arena and auto-dealocating and create alternative to create on stack
Arena_mark* arena_mark_on_arena(Arena* arena)
{
    assert(arena != NULL);
    Page last_page_snap = {0};
    if (arena->end != NULL) {
        memcpy(&last_page_snap, arena->end, sizeof(Page));
    }
    Arena arena_snap = {0};
    memcpy(&arena_snap, arena, sizeof(Arena));

    // TODO: copying data 2 times, is this ok?
    Arena_mark* mark = arena_alloc(arena, sizeof(Arena_mark));
    mark->mark_for = arena;
    mark->arena = arena_snap;
    mark->last_page = last_page_snap;

    return mark;
}

Arena_mark arena_mark(Arena* arena)
{
    assert(arena != NULL);

    Arena_mark mark = {0};
    mark.mark_for = arena;
    mark.arena = *arena;
    if (arena->end != NULL) {
        mark.last_page = *(arena->end);
    }

    return mark;
}

void arena_restore_flag(Arena* arena, Arena_mark* mark, int flags)
{
    assert(arena != NULL);
    assert(mark != NULL);
    if (mark->mark_for != arena) {
        fprintf(stderr, MARENA_STR_PREFIX": Attempt to restore arena(%p) from mark, that doesnt belongs to it (mark for %p)\n", arena, mark->mark_for);
        exit(1);
    }

    if ((arena->start == NULL && mark->arena.start != NULL) || (arena->end == NULL && mark->arena.end != NULL)) {
        fprintf(stderr, MARENA_STR_PREFIX": Attemp to restore uninitialized arena or memory that was previously freed\n");
        exit(1);
    }

    if (mark->arena.start == NULL || mark->arena.end == NULL) {
        arena__reset_flag(arena, flags);
        return;
    }

    memcpy(arena, &(mark->arena), sizeof(Arena));

    // restore page metadata, dont touch 'next' pointer, so not to leak memory
    Page* end_page = arena->end;
    end_page->start_free = mark->last_page.start_free;
    end_page->allocated = mark->last_page.allocated;
    end_page->size = mark->last_page.size;  // TODO: maybe dont restore size? It wont change in any case.


    // reset all what were allocated after mark creation
    for (Page* p = arena->end; p != NULL; p = p->next) {
        arena__reset_page_flag(p, flags);
    }
}

void marena__print_arena(Arena* arena)
{
    printf(" >> Arena %p\n", arena);
    printf("start: %p\n", arena->start);
    printf("end: %p\n", arena->end);
    printf("page_size: %zu\n", arena->page_size);
    printf("flags: %d\n", arena->flags);
    printf("_pagemiss_cnt: %llu\n", arena->_pagemiss_cnt);
    printf("_allocs_cnt: %llu\n", arena->_allocs_cnt);
#ifdef MARENA_DEBUG
    printf("_page_size_grows_cnt: %llu\n", a->_page_size_grows_cnt);
#endif
    for (Page* p = arena->start; p != NULL; p = p->next) {
        printf("| %s%s page: %p, end: %p, size: %zu, allocated: %zu (free ptr: %p), next: %p | V\n", 
            p == arena->start ? "(start)" : "", p == arena->end ? "(end)" : "",
            p, 
            (char*)p + p->size + sizeof(Page), 
            p->size,
            p->allocated,
            p->start_free, 
            p->next
        );
    }
}
#endif // MARENA_IMPLEMENTATION

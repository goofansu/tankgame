/*
 * Tank Game - Memory Management Implementation
 */

#include "pz_mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// In debug builds, track allocations with metadata
#ifdef PZ_DEBUG

typedef struct pz_alloc_header {
    size_t size;
    pz_mem_category category;
    struct pz_alloc_header* next;
    struct pz_alloc_header* prev;
    const char* file;   // Future: track source location
    int line;
    uint32_t magic;     // 0xDEADBEEF for validity check
} pz_alloc_header;

#define PZ_ALLOC_MAGIC 0xDEADBEEF

// Global state (only in debug)
static pz_alloc_header* g_alloc_list = NULL;
static size_t g_total_allocated = 0;
static size_t g_alloc_count = 0;
static size_t g_category_allocated[PZ_MEM_CATEGORY_COUNT] = {0};
static bool g_mem_initialized = false;

void pz_mem_init(void) {
    g_alloc_list = NULL;
    g_total_allocated = 0;
    g_alloc_count = 0;
    memset(g_category_allocated, 0, sizeof(g_category_allocated));
    g_mem_initialized = true;
}

void pz_mem_shutdown(void) {
    pz_mem_dump_leaks();
    g_mem_initialized = false;
}

static void* pz_alloc_internal(size_t size, pz_mem_category category) {
    if (size == 0) return NULL;
    
    // Allocate header + user data
    pz_alloc_header* header = (pz_alloc_header*)malloc(sizeof(pz_alloc_header) + size);
    if (!header) return NULL;
    
    header->size = size;
    header->category = category;
    header->magic = PZ_ALLOC_MAGIC;
    header->file = NULL;
    header->line = 0;
    
    // Insert at head of list
    header->next = g_alloc_list;
    header->prev = NULL;
    if (g_alloc_list) {
        g_alloc_list->prev = header;
    }
    g_alloc_list = header;
    
    // Update stats
    g_total_allocated += size;
    g_alloc_count++;
    g_category_allocated[category] += size;
    
    // Return pointer past header
    return (void*)(header + 1);
}

void* pz_alloc(size_t size) {
    return pz_alloc_internal(size, PZ_MEM_GENERAL);
}

void* pz_alloc_tagged(size_t size, pz_mem_category category) {
    return pz_alloc_internal(size, category);
}

void* pz_calloc(size_t count, size_t size) {
    return pz_calloc_tagged(count, size, PZ_MEM_GENERAL);
}

void* pz_calloc_tagged(size_t count, size_t size, pz_mem_category category) {
    size_t total = count * size;
    void* ptr = pz_alloc_internal(total, category);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* pz_realloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return pz_alloc(new_size);
    }
    if (new_size == 0) {
        pz_free(ptr);
        return NULL;
    }
    
    // Get existing header
    pz_alloc_header* header = ((pz_alloc_header*)ptr) - 1;
    if (header->magic != PZ_ALLOC_MAGIC) {
        fprintf(stderr, "pz_realloc: invalid pointer or memory corruption!\n");
        return NULL;
    }
    
    size_t old_size = header->size;
    pz_mem_category category = header->category;
    
    // Remove from list
    if (header->prev) header->prev->next = header->next;
    else g_alloc_list = header->next;
    if (header->next) header->next->prev = header->prev;
    
    // Update stats for removal
    g_total_allocated -= old_size;
    g_category_allocated[category] -= old_size;
    
    // Realloc
    pz_alloc_header* new_header = (pz_alloc_header*)realloc(header, sizeof(pz_alloc_header) + new_size);
    if (!new_header) {
        // Restore old allocation to list on failure
        header->next = g_alloc_list;
        header->prev = NULL;
        if (g_alloc_list) g_alloc_list->prev = header;
        g_alloc_list = header;
        g_total_allocated += old_size;
        g_category_allocated[category] += old_size;
        return NULL;
    }
    
    new_header->size = new_size;
    
    // Re-insert at head
    new_header->next = g_alloc_list;
    new_header->prev = NULL;
    if (g_alloc_list) g_alloc_list->prev = new_header;
    g_alloc_list = new_header;
    
    // Update stats
    g_total_allocated += new_size;
    g_category_allocated[category] += new_size;
    
    return (void*)(new_header + 1);
}

void pz_free(void* ptr) {
    if (!ptr) return;
    
    pz_alloc_header* header = ((pz_alloc_header*)ptr) - 1;
    if (header->magic != PZ_ALLOC_MAGIC) {
        fprintf(stderr, "pz_free: invalid pointer or double-free!\n");
        return;
    }
    
    // Remove from list
    if (header->prev) header->prev->next = header->next;
    else g_alloc_list = header->next;
    if (header->next) header->next->prev = header->prev;
    
    // Update stats
    g_total_allocated -= header->size;
    g_alloc_count--;
    g_category_allocated[header->category] -= header->size;
    
    // Invalidate magic to catch double-free
    header->magic = 0;
    
    free(header);
}

size_t pz_mem_get_allocated(void) {
    return g_total_allocated;
}

size_t pz_mem_get_alloc_count(void) {
    return g_alloc_count;
}

size_t pz_mem_get_category_allocated(pz_mem_category category) {
    if (category >= PZ_MEM_CATEGORY_COUNT) return 0;
    return g_category_allocated[category];
}

void pz_mem_dump_leaks(void) {
    if (!g_alloc_list) {
        fprintf(stderr, "[Memory] No leaks detected. Total allocs: 0\n");
        return;
    }
    
    fprintf(stderr, "\n[Memory] LEAK REPORT:\n");
    fprintf(stderr, "------------------------\n");
    
    static const char* category_names[] = {
        "GENERAL", "RENDER", "AUDIO", "GAME", "NETWORK", "TEMP"
    };
    
    int leak_count = 0;
    size_t leaked_bytes = 0;
    
    for (pz_alloc_header* h = g_alloc_list; h; h = h->next) {
        leak_count++;
        leaked_bytes += h->size;
        fprintf(stderr, "  Leak #%d: %zu bytes (%s)\n", 
                leak_count, h->size, category_names[h->category]);
    }
    
    fprintf(stderr, "------------------------\n");
    fprintf(stderr, "Total: %d leaks, %zu bytes\n\n", leak_count, leaked_bytes);
}

bool pz_mem_has_leaks(void) {
    return g_alloc_list != NULL;
}

#else // Release builds - thin wrappers around malloc/free

static size_t g_total_allocated = 0;
static size_t g_alloc_count = 0;

void pz_mem_init(void) {
    g_total_allocated = 0;
    g_alloc_count = 0;
}

void pz_mem_shutdown(void) {
    // No-op in release
}

void* pz_alloc(size_t size) {
    if (size == 0) return NULL;
    g_total_allocated += size;
    g_alloc_count++;
    return malloc(size);
}

void* pz_alloc_tagged(size_t size, pz_mem_category category) {
    (void)category;
    return pz_alloc(size);
}

void* pz_calloc(size_t count, size_t size) {
    g_total_allocated += count * size;
    g_alloc_count++;
    return calloc(count, size);
}

void* pz_calloc_tagged(size_t count, size_t size, pz_mem_category category) {
    (void)category;
    return pz_calloc(count, size);
}

void* pz_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

void pz_free(void* ptr) {
    if (ptr) {
        g_alloc_count--;
    }
    free(ptr);
}

size_t pz_mem_get_allocated(void) {
    return g_total_allocated;
}

size_t pz_mem_get_alloc_count(void) {
    return g_alloc_count;
}

size_t pz_mem_get_category_allocated(pz_mem_category category) {
    (void)category;
    return 0; // Not tracked in release
}

void pz_mem_dump_leaks(void) {
    // No-op in release
}

bool pz_mem_has_leaks(void) {
    return false; // Can't know in release
}

#endif // PZ_DEBUG

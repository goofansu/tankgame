/*
 * Tank Game - Data Structures
 *
 * Provides:
 * - pz_list: intrusive doubly-linked list
 * - pz_array: stretchy buffer (stb-style dynamic array)
 * - pz_hashmap: string-keyed hash map
 */

#ifndef PZ_DS_H
#define PZ_DS_H

#include "pz_mem.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Intrusive Doubly-Linked List
 *
 * Usage:
 *   struct my_node {
 *       int value;
 *       pz_list_node link;  // embed this in your struct
 *   };
 *
 *   pz_list list;
 *   pz_list_init(&list);
 *
 *   struct my_node *node = pz_alloc(sizeof(*node));
 *   node->value = 42;
 *   pz_list_push_back(&list, &node->link);
 *
 *   pz_list_for_each(&list, cur) {
 *       struct my_node *n = pz_list_entry(cur, struct my_node, link);
 *       printf("%d\n", n->value);
 *   }
 * ============================================================================
 */

typedef struct pz_list_node {
    struct pz_list_node *prev;
    struct pz_list_node *next;
} pz_list_node;

typedef struct pz_list {
    pz_list_node head; // sentinel node
    size_t count;
} pz_list;

// Initialize list
void pz_list_init(pz_list *list);

// Check if empty
bool pz_list_empty(const pz_list *list);

// Get count
size_t pz_list_count(const pz_list *list);

// Push to front/back
void pz_list_push_front(pz_list *list, pz_list_node *node);
void pz_list_push_back(pz_list *list, pz_list_node *node);

// Pop from front/back (returns NULL if empty)
pz_list_node *pz_list_pop_front(pz_list *list);
pz_list_node *pz_list_pop_back(pz_list *list);

// Remove specific node
void pz_list_remove(pz_list *list, pz_list_node *node);

// Get first/last (returns NULL if empty)
pz_list_node *pz_list_first(const pz_list *list);
pz_list_node *pz_list_last(const pz_list *list);

// Insert before/after a node
void pz_list_insert_before(
    pz_list *list, pz_list_node *before, pz_list_node *node);
void pz_list_insert_after(
    pz_list *list, pz_list_node *after, pz_list_node *node);

// Get containing struct from list node
// Example: pz_list_entry(node_ptr, struct my_type, link_field)
#define pz_list_entry(ptr, type, member)                                       \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Iterate over all nodes
// Usage: pz_list_for_each(list_ptr, iter_name) { ... }
#define pz_list_for_each(list, iter)                                           \
    for (pz_list_node *iter = (list)->head.next; iter != &(list)->head;        \
         iter = iter->next)

// Iterate with safe removal
#define pz_list_for_each_safe(list, iter, tmp)                                 \
    for (pz_list_node *iter = (list)->head.next, *tmp = iter->next;            \
         iter != &(list)->head; iter = tmp, tmp = iter->next)

/* ============================================================================
 * Stretchy Buffer (Dynamic Array)
 *
 * stb-style stretchy buffer. Works with any type via macros.
 *
 * Usage:
 *   int *arr = NULL;  // must initialize to NULL
 *
 *   pz_array_push(arr, 10);
 *   pz_array_push(arr, 20);
 *   pz_array_push(arr, 30);
 *
 *   for (size_t i = 0; i < pz_array_len(arr); i++) {
 *       printf("%d\n", arr[i]);
 *   }
 *
 *   pz_array_free(arr);
 * ============================================================================
 */

// Internal header stored before array data
typedef struct pz_array_header {
    size_t len;
    size_t cap;
} pz_array_header;

// Get header from array pointer
#define pz_array__header(a)                                                    \
    ((pz_array_header *)((char *)(a) - sizeof(pz_array_header)))

// Get length (0 if NULL)
#define pz_array_len(a) ((a) ? pz_array__header(a)->len : 0)

// Get capacity (0 if NULL)
#define pz_array_cap(a) ((a) ? pz_array__header(a)->cap : 0)

// Check if empty
#define pz_array_empty(a) (pz_array_len(a) == 0)

// Push element to end
#define pz_array_push(a, v)                                                    \
    (pz_array__maybe_grow(a, 1), (a)[pz_array__header(a)->len++] = (v))

// Pop element from end (undefined if empty)
#define pz_array_pop(a) ((a)[--pz_array__header(a)->len])

// Get last element (undefined if empty)
#define pz_array_last(a) ((a)[pz_array__header(a)->len - 1])

// Clear array (keep capacity)
#define pz_array_clear(a)                                                      \
    do {                                                                       \
        if (a)                                                                 \
            pz_array__header(a)->len = 0;                                      \
    } while (0)

// Free array
#define pz_array_free(a)                                                       \
    do {                                                                       \
        if (a) {                                                               \
            pz_free(pz_array__header(a));                                      \
            (a) = NULL;                                                        \
        }                                                                      \
    } while (0)

// Reserve capacity
#define pz_array_reserve(a, n) pz_array__maybe_grow(a, n)

// Resize to exact length (grows if needed, doesn't initialize new elements)
#define pz_array_resize(a, n)                                                  \
    do {                                                                       \
        pz_array__maybe_grow(a, (n) - pz_array_len(a));                        \
        pz_array__header(a)->len = (n);                                        \
    } while (0)

// Insert at index (shifts elements right)
#define pz_array_insert(a, idx, v)                                             \
    do {                                                                       \
        pz_array__maybe_grow(a, 1);                                            \
        size_t _idx = (idx);                                                   \
        size_t _len = pz_array__header(a)->len;                                \
        if (_idx < _len) {                                                     \
            memmove(&(a)[_idx + 1], &(a)[_idx], (_len - _idx) * sizeof(*(a))); \
        }                                                                      \
        (a)[_idx] = (v);                                                       \
        pz_array__header(a)->len++;                                            \
    } while (0)

// Remove at index (shifts elements left)
#define pz_array_remove(a, idx)                                                \
    do {                                                                       \
        size_t _idx = (idx);                                                   \
        size_t _len = pz_array__header(a)->len;                                \
        if (_idx < _len - 1) {                                                 \
            memmove(                                                           \
                &(a)[_idx], &(a)[_idx + 1], (_len - _idx - 1) * sizeof(*(a))); \
        }                                                                      \
        pz_array__header(a)->len--;                                            \
    } while (0)

// Remove at index by swapping with last (O(1) but doesn't preserve order)
#define pz_array_remove_swap(a, idx)                                           \
    do {                                                                       \
        size_t _idx = (idx);                                                   \
        size_t _len = pz_array__header(a)->len;                                \
        if (_idx < _len - 1) {                                                 \
            (a)[_idx] = (a)[_len - 1];                                         \
        }                                                                      \
        pz_array__header(a)->len--;                                            \
    } while (0)

// Internal: grow if needed
#define pz_array__maybe_grow(a, n)                                             \
    pz_array__grow((void **)&(a), (n), sizeof(*(a)))

// Internal: actual grow function
void pz_array__grow(void **arr, size_t add_count, size_t elem_size);

/* ============================================================================
 * Hash Map (String Keys)
 *
 * Hash map with string keys and void* values.
 *
 * Usage:
 *   pz_hashmap map;
 *   pz_hashmap_init(&map, 16);  // initial capacity
 *
 *   pz_hashmap_set(&map, "key1", value1);
 *   pz_hashmap_set(&map, "key2", value2);
 *
 *   void *v = pz_hashmap_get(&map, "key1");
 *   if (pz_hashmap_has(&map, "key2")) { ... }
 *
 *   pz_hashmap_remove(&map, "key1");
 *
 *   pz_hashmap_for_each(&map, key, val) {
 *       printf("%s -> %p\n", key, val);
 *   }
 *
 *   pz_hashmap_destroy(&map);
 * ============================================================================
 */

typedef struct pz_hashmap_entry {
    char *key; // NULL if slot is empty
    void *value;
    uint32_t hash; // cached hash
    bool deleted; // tombstone marker
} pz_hashmap_entry;

typedef struct pz_hashmap {
    pz_hashmap_entry *entries;
    size_t capacity;
    size_t count; // active entries
    size_t tombstones; // deleted entries
} pz_hashmap;

// Initialize with initial capacity (will be rounded up to power of 2)
void pz_hashmap_init(pz_hashmap *map, size_t initial_capacity);

// Destroy and free all memory
void pz_hashmap_destroy(pz_hashmap *map);

// Clear all entries (keep capacity)
void pz_hashmap_clear(pz_hashmap *map);

// Get number of entries
size_t pz_hashmap_count(const pz_hashmap *map);

// Check if key exists
bool pz_hashmap_has(const pz_hashmap *map, const char *key);

// Get value for key (returns NULL if not found)
void *pz_hashmap_get(const pz_hashmap *map, const char *key);

// Set key to value (overwrites if exists)
// Key is copied internally
void pz_hashmap_set(pz_hashmap *map, const char *key, void *value);

// Remove key (returns previous value, or NULL if not found)
void *pz_hashmap_remove(pz_hashmap *map, const char *key);

// Iterate over all entries
// Usage: pz_hashmap_for_each(&map, key_var, val_var) { ... }
#define pz_hashmap_for_each(map, key_var, val_var)                             \
    for (size_t _i = 0, _found = 0; _i < (map)->capacity; _i++)                \
        if ((map)->entries[_i].key != NULL && !(map)->entries[_i].deleted)     \
            for (const char *key_var = (map)->entries[_i].key,                 \
                            *_dummy = (const char *)(_found = 1);              \
                 _found; _found = 0)                                           \
                for (void *val_var = (map)->entries[_i].value; _dummy;         \
                     _dummy = NULL)

// Hash function (FNV-1a)
uint32_t pz_hash_string(const char *str);

#endif // PZ_DS_H

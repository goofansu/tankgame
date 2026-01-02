/*
 * Tank Game - Data Structures Implementation
 */

#include "pz_ds.h"
#include <string.h>

/* ============================================================================
 * Intrusive Doubly-Linked List
 * ============================================================================
 */

void
pz_list_init(pz_list *list)
{
    list->head.prev = &list->head;
    list->head.next = &list->head;
    list->count = 0;
}

bool
pz_list_empty(const pz_list *list)
{
    return list->head.next == &list->head;
}

size_t
pz_list_count(const pz_list *list)
{
    return list->count;
}

void
pz_list_push_front(pz_list *list, pz_list_node *node)
{
    node->prev = &list->head;
    node->next = list->head.next;
    list->head.next->prev = node;
    list->head.next = node;
    list->count++;
}

void
pz_list_push_back(pz_list *list, pz_list_node *node)
{
    node->prev = list->head.prev;
    node->next = &list->head;
    list->head.prev->next = node;
    list->head.prev = node;
    list->count++;
}

pz_list_node *
pz_list_pop_front(pz_list *list)
{
    if (pz_list_empty(list)) {
        return NULL;
    }
    pz_list_node *node = list->head.next;
    pz_list_remove(list, node);
    return node;
}

pz_list_node *
pz_list_pop_back(pz_list *list)
{
    if (pz_list_empty(list)) {
        return NULL;
    }
    pz_list_node *node = list->head.prev;
    pz_list_remove(list, node);
    return node;
}

void
pz_list_remove(pz_list *list, pz_list_node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = NULL;
    node->next = NULL;
    list->count--;
}

pz_list_node *
pz_list_first(const pz_list *list)
{
    if (pz_list_empty(list)) {
        return NULL;
    }
    return list->head.next;
}

pz_list_node *
pz_list_last(const pz_list *list)
{
    if (pz_list_empty(list)) {
        return NULL;
    }
    return list->head.prev;
}

void
pz_list_insert_before(pz_list *list, pz_list_node *before, pz_list_node *node)
{
    node->prev = before->prev;
    node->next = before;
    before->prev->next = node;
    before->prev = node;
    list->count++;
}

void
pz_list_insert_after(pz_list *list, pz_list_node *after, pz_list_node *node)
{
    node->prev = after;
    node->next = after->next;
    after->next->prev = node;
    after->next = node;
    list->count++;
}

/* ============================================================================
 * Stretchy Buffer (Dynamic Array)
 * ============================================================================
 */

void
pz_array__grow(void **arr, size_t add_count, size_t elem_size)
{
    pz_array_header *header;
    size_t new_len, new_cap;

    if (*arr == NULL) {
        // Initial allocation
        new_cap = add_count > 8 ? add_count : 8;
        header = pz_alloc(sizeof(pz_array_header) + new_cap * elem_size);
        header->len = 0;
        header->cap = new_cap;
        *arr = (char *)header + sizeof(pz_array_header);
    } else {
        header = pz_array__header(*arr);
        new_len = header->len + add_count;

        if (new_len > header->cap) {
            // Need to grow
            new_cap = header->cap * 2;
            if (new_cap < new_len) {
                new_cap = new_len;
            }

            header = pz_realloc(
                header, sizeof(pz_array_header) + new_cap * elem_size);
            header->cap = new_cap;
            *arr = (char *)header + sizeof(pz_array_header);
        }
    }
}

/* ============================================================================
 * Hash Map
 * ============================================================================
 */

// FNV-1a hash
uint32_t
pz_hash_string(const char *str)
{
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

// Round up to next power of 2
static size_t
next_pow2(size_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if SIZE_MAX > 0xFFFFFFFF
    n |= n >> 32;
#endif
    return n + 1;
}

void
pz_hashmap_init(pz_hashmap *map, size_t initial_capacity)
{
    if (initial_capacity < 8) {
        initial_capacity = 8;
    }
    initial_capacity = next_pow2(initial_capacity);

    map->entries = pz_calloc(initial_capacity, sizeof(pz_hashmap_entry));
    map->capacity = initial_capacity;
    map->count = 0;
    map->tombstones = 0;
}

void
pz_hashmap_destroy(pz_hashmap *map)
{
    // Free all keys
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].key != NULL) {
            pz_free(map->entries[i].key);
        }
    }
    pz_free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->count = 0;
    map->tombstones = 0;
}

void
pz_hashmap_clear(pz_hashmap *map)
{
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].key != NULL) {
            pz_free(map->entries[i].key);
            map->entries[i].key = NULL;
        }
        map->entries[i].deleted = false;
    }
    map->count = 0;
    map->tombstones = 0;
}

size_t
pz_hashmap_count(const pz_hashmap *map)
{
    return map->count;
}

// Find slot for key (returns index of slot)
// If found: entries[result].key matches and !deleted
// If not found: entries[result].key is NULL or we should insert at first
// tombstone
static size_t
pz_hashmap_find_slot(const pz_hashmap *map, const char *key, uint32_t hash)
{
    size_t mask = map->capacity - 1;
    size_t idx = hash & mask;
    size_t first_tombstone = SIZE_MAX;

    for (;;) {
        pz_hashmap_entry *entry = &map->entries[idx];

        if (entry->key == NULL) {
            // Empty slot - key not found
            if (!entry->deleted) {
                // True empty slot
                return (first_tombstone != SIZE_MAX) ? first_tombstone : idx;
            } else {
                // Tombstone
                if (first_tombstone == SIZE_MAX) {
                    first_tombstone = idx;
                }
            }
        } else if (!entry->deleted && entry->hash == hash
            && strcmp(entry->key, key) == 0) {
            // Found the key
            return idx;
        }

        // Linear probing
        idx = (idx + 1) & mask;
    }
}

static void
pz_hashmap_resize(pz_hashmap *map, size_t new_capacity)
{
    pz_hashmap_entry *old_entries = map->entries;
    size_t old_capacity = map->capacity;

    map->entries = pz_calloc(new_capacity, sizeof(pz_hashmap_entry));
    map->capacity = new_capacity;
    map->count = 0;
    map->tombstones = 0;

    // Rehash all entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].key != NULL && !old_entries[i].deleted) {
            size_t idx = pz_hashmap_find_slot(
                map, old_entries[i].key, old_entries[i].hash);
            map->entries[idx].key = old_entries[i].key;
            map->entries[idx].value = old_entries[i].value;
            map->entries[idx].hash = old_entries[i].hash;
            map->entries[idx].deleted = false;
            map->count++;
        } else if (old_entries[i].key != NULL) {
            // Free tombstone keys
            pz_free(old_entries[i].key);
        }
    }

    pz_free(old_entries);
}

bool
pz_hashmap_has(const pz_hashmap *map, const char *key)
{
    uint32_t hash = pz_hash_string(key);
    size_t idx = pz_hashmap_find_slot(map, key, hash);
    return map->entries[idx].key != NULL && !map->entries[idx].deleted;
}

void *
pz_hashmap_get(const pz_hashmap *map, const char *key)
{
    uint32_t hash = pz_hash_string(key);
    size_t idx = pz_hashmap_find_slot(map, key, hash);

    if (map->entries[idx].key != NULL && !map->entries[idx].deleted) {
        return map->entries[idx].value;
    }
    return NULL;
}

void
pz_hashmap_set(pz_hashmap *map, const char *key, void *value)
{
    // Resize if load factor > 0.7 (including tombstones)
    if ((map->count + map->tombstones + 1) * 10 > map->capacity * 7) {
        pz_hashmap_resize(map, map->capacity * 2);
    }

    uint32_t hash = pz_hash_string(key);
    size_t idx = pz_hashmap_find_slot(map, key, hash);
    pz_hashmap_entry *entry = &map->entries[idx];

    if (entry->key != NULL && !entry->deleted) {
        // Update existing
        entry->value = value;
    } else {
        // New entry
        bool was_tombstone = entry->deleted;
        if (entry->key != NULL) {
            pz_free(entry->key); // Free tombstone key if any
        }

        size_t key_len = strlen(key);
        entry->key = pz_alloc(key_len + 1);
        memcpy(entry->key, key, key_len + 1);
        entry->value = value;
        entry->hash = hash;
        entry->deleted = false;
        map->count++;

        if (was_tombstone) {
            map->tombstones--;
        }
    }
}

void *
pz_hashmap_remove(pz_hashmap *map, const char *key)
{
    uint32_t hash = pz_hash_string(key);
    size_t idx = pz_hashmap_find_slot(map, key, hash);
    pz_hashmap_entry *entry = &map->entries[idx];

    if (entry->key == NULL || entry->deleted) {
        return NULL; // Not found
    }

    void *value = entry->value;
    entry->deleted = true;
    entry->value = NULL;
    map->count--;
    map->tombstones++;

    // If too many tombstones, rebuild
    if (map->tombstones > map->capacity / 4) {
        pz_hashmap_resize(map, map->capacity);
    }

    return value;
}

/*
 * Tank Game - Data Structure Tests
 */

#include "../src/core/pz_ds.h"
#include "../src/core/pz_mem.h"
#include "test_framework.h"
#include <string.h>

/* ============================================================================
 * List Tests
 * ============================================================================
 */

typedef struct test_node {
    int value;
    pz_list_node link;
} test_node;

TEST(list_init_empty)
{
    pz_list list;
    pz_list_init(&list);

    ASSERT(pz_list_empty(&list));
    ASSERT_EQ(0, pz_list_count(&list));
    ASSERT_NULL(pz_list_first(&list));
    ASSERT_NULL(pz_list_last(&list));
}

TEST(list_push_back)
{
    pz_mem_init();
    pz_list list;
    pz_list_init(&list);

    test_node *n1 = pz_alloc(sizeof(test_node));
    test_node *n2 = pz_alloc(sizeof(test_node));
    test_node *n3 = pz_alloc(sizeof(test_node));
    n1->value = 1;
    n2->value = 2;
    n3->value = 3;

    pz_list_push_back(&list, &n1->link);
    pz_list_push_back(&list, &n2->link);
    pz_list_push_back(&list, &n3->link);

    ASSERT(!pz_list_empty(&list));
    ASSERT_EQ(3, pz_list_count(&list));

    // Check order: 1, 2, 3
    test_node *first = pz_list_entry(pz_list_first(&list), test_node, link);
    test_node *last = pz_list_entry(pz_list_last(&list), test_node, link);
    ASSERT_EQ(1, first->value);
    ASSERT_EQ(3, last->value);

    pz_free(n1);
    pz_free(n2);
    pz_free(n3);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(list_push_front)
{
    pz_mem_init();
    pz_list list;
    pz_list_init(&list);

    test_node *n1 = pz_alloc(sizeof(test_node));
    test_node *n2 = pz_alloc(sizeof(test_node));
    test_node *n3 = pz_alloc(sizeof(test_node));
    n1->value = 1;
    n2->value = 2;
    n3->value = 3;

    pz_list_push_front(&list, &n1->link);
    pz_list_push_front(&list, &n2->link);
    pz_list_push_front(&list, &n3->link);

    // Check order: 3, 2, 1 (reverse of insertion)
    test_node *first = pz_list_entry(pz_list_first(&list), test_node, link);
    test_node *last = pz_list_entry(pz_list_last(&list), test_node, link);
    ASSERT_EQ(3, first->value);
    ASSERT_EQ(1, last->value);

    pz_free(n1);
    pz_free(n2);
    pz_free(n3);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(list_pop_front_back)
{
    pz_mem_init();
    pz_list list;
    pz_list_init(&list);

    test_node *n1 = pz_alloc(sizeof(test_node));
    test_node *n2 = pz_alloc(sizeof(test_node));
    test_node *n3 = pz_alloc(sizeof(test_node));
    n1->value = 1;
    n2->value = 2;
    n3->value = 3;

    pz_list_push_back(&list, &n1->link);
    pz_list_push_back(&list, &n2->link);
    pz_list_push_back(&list, &n3->link);

    // Pop front
    pz_list_node *popped = pz_list_pop_front(&list);
    ASSERT_NOT_NULL(popped);
    ASSERT_EQ(1, pz_list_entry(popped, test_node, link)->value);
    ASSERT_EQ(2, pz_list_count(&list));

    // Pop back
    popped = pz_list_pop_back(&list);
    ASSERT_NOT_NULL(popped);
    ASSERT_EQ(3, pz_list_entry(popped, test_node, link)->value);
    ASSERT_EQ(1, pz_list_count(&list));

    // Pop remaining
    popped = pz_list_pop_front(&list);
    ASSERT_EQ(2, pz_list_entry(popped, test_node, link)->value);
    ASSERT(pz_list_empty(&list));

    // Pop from empty
    ASSERT_NULL(pz_list_pop_front(&list));
    ASSERT_NULL(pz_list_pop_back(&list));

    pz_free(n1);
    pz_free(n2);
    pz_free(n3);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(list_remove)
{
    pz_mem_init();
    pz_list list;
    pz_list_init(&list);

    test_node *n1 = pz_alloc(sizeof(test_node));
    test_node *n2 = pz_alloc(sizeof(test_node));
    test_node *n3 = pz_alloc(sizeof(test_node));
    n1->value = 1;
    n2->value = 2;
    n3->value = 3;

    pz_list_push_back(&list, &n1->link);
    pz_list_push_back(&list, &n2->link);
    pz_list_push_back(&list, &n3->link);

    // Remove middle element
    pz_list_remove(&list, &n2->link);
    ASSERT_EQ(2, pz_list_count(&list));

    // Verify: 1, 3
    test_node *first = pz_list_entry(pz_list_first(&list), test_node, link);
    test_node *last = pz_list_entry(pz_list_last(&list), test_node, link);
    ASSERT_EQ(1, first->value);
    ASSERT_EQ(3, last->value);

    pz_free(n1);
    pz_free(n2);
    pz_free(n3);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(list_iterate)
{
    pz_mem_init();
    pz_list list;
    pz_list_init(&list);

    test_node nodes[5];
    for (int i = 0; i < 5; i++) {
        nodes[i].value = i;
        pz_list_push_back(&list, &nodes[i].link);
    }

    // Iterate and sum values
    int sum = 0;
    pz_list_for_each(&list, cur)
    {
        test_node *n = pz_list_entry(cur, test_node, link);
        sum += n->value;
    }
    ASSERT_EQ(0 + 1 + 2 + 3 + 4, sum);

    pz_mem_shutdown();
}

TEST(list_iterate_safe_remove)
{
    pz_mem_init();
    pz_list list;
    pz_list_init(&list);

    test_node *nodes[5];
    for (int i = 0; i < 5; i++) {
        nodes[i] = pz_alloc(sizeof(test_node));
        nodes[i]->value = i;
        pz_list_push_back(&list, &nodes[i]->link);
    }

    // Remove even elements during iteration
    pz_list_for_each_safe(&list, cur, tmp)
    {
        test_node *n = pz_list_entry(cur, test_node, link);
        if (n->value % 2 == 0) {
            pz_list_remove(&list, cur);
        }
    }

    // Should have 1, 3 remaining
    ASSERT_EQ(2, pz_list_count(&list));

    int sum = 0;
    pz_list_for_each(&list, cur)
    {
        test_node *n = pz_list_entry(cur, test_node, link);
        sum += n->value;
    }
    ASSERT_EQ(1 + 3, sum);

    for (int i = 0; i < 5; i++) {
        pz_free(nodes[i]);
    }
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

/* ============================================================================
 * Array Tests
 * ============================================================================
 */

TEST(array_push_pop)
{
    pz_mem_init();

    int *arr = NULL;

    pz_array_push(arr, 10);
    pz_array_push(arr, 20);
    pz_array_push(arr, 30);

    ASSERT_EQ(3, pz_array_len(arr));
    ASSERT_EQ(10, arr[0]);
    ASSERT_EQ(20, arr[1]);
    ASSERT_EQ(30, arr[2]);
    ASSERT_EQ(30, pz_array_last(arr));

    int popped = pz_array_pop(arr);
    ASSERT_EQ(30, popped);
    ASSERT_EQ(2, pz_array_len(arr));

    pz_array_free(arr);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(array_growth)
{
    pz_mem_init();

    int *arr = NULL;

    // Push many elements to trigger multiple growths
    for (int i = 0; i < 100; i++) {
        pz_array_push(arr, i);
    }

    ASSERT_EQ(100, pz_array_len(arr));
    ASSERT(pz_array_cap(arr) >= 100);

    // Verify all values
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(i, arr[i]);
    }

    pz_array_free(arr);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(array_clear)
{
    pz_mem_init();

    int *arr = NULL;
    pz_array_push(arr, 1);
    pz_array_push(arr, 2);
    pz_array_push(arr, 3);

    size_t old_cap = pz_array_cap(arr);
    pz_array_clear(arr);

    ASSERT_EQ(0, pz_array_len(arr));
    ASSERT_EQ(old_cap, pz_array_cap(arr)); // Capacity preserved

    pz_array_free(arr);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(array_null_operations)
{
    // Operations on NULL array shouldn't crash
    int *arr = NULL;

    ASSERT_EQ(0, pz_array_len(arr));
    ASSERT_EQ(0, pz_array_cap(arr));
    ASSERT(pz_array_empty(arr));

    pz_array_free(arr); // Should be safe
    pz_array_clear(arr); // Should be safe
}

TEST(array_insert_remove)
{
    pz_mem_init();

    int *arr = NULL;
    pz_array_push(arr, 1);
    pz_array_push(arr, 2);
    pz_array_push(arr, 4);
    pz_array_push(arr, 5);

    // Insert 3 at index 2
    pz_array_insert(arr, 2, 3);
    ASSERT_EQ(5, pz_array_len(arr));
    ASSERT_EQ(1, arr[0]);
    ASSERT_EQ(2, arr[1]);
    ASSERT_EQ(3, arr[2]);
    ASSERT_EQ(4, arr[3]);
    ASSERT_EQ(5, arr[4]);

    // Remove at index 2
    pz_array_remove(arr, 2);
    ASSERT_EQ(4, pz_array_len(arr));
    ASSERT_EQ(1, arr[0]);
    ASSERT_EQ(2, arr[1]);
    ASSERT_EQ(4, arr[2]);
    ASSERT_EQ(5, arr[3]);

    pz_array_free(arr);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(array_remove_swap)
{
    pz_mem_init();

    int *arr = NULL;
    pz_array_push(arr, 1);
    pz_array_push(arr, 2);
    pz_array_push(arr, 3);
    pz_array_push(arr, 4);

    // Remove index 1 by swap (2 gets replaced by 4)
    pz_array_remove_swap(arr, 1);
    ASSERT_EQ(3, pz_array_len(arr));
    ASSERT_EQ(1, arr[0]);
    ASSERT_EQ(4, arr[1]); // Last element moved here
    ASSERT_EQ(3, arr[2]);

    pz_array_free(arr);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(array_struct)
{
    pz_mem_init();

    typedef struct {
        int x, y;
    } point;

    point *points = NULL;

    pz_array_push(points, ((point) { 1, 2 }));
    pz_array_push(points, ((point) { 3, 4 }));
    pz_array_push(points, ((point) { 5, 6 }));

    ASSERT_EQ(3, pz_array_len(points));
    ASSERT_EQ(1, points[0].x);
    ASSERT_EQ(2, points[0].y);
    ASSERT_EQ(3, points[1].x);
    ASSERT_EQ(4, points[1].y);
    ASSERT_EQ(5, points[2].x);
    ASSERT_EQ(6, points[2].y);

    pz_array_free(points);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

/* ============================================================================
 * Hashmap Tests
 * ============================================================================
 */

TEST(hashmap_basic)
{
    pz_mem_init();

    pz_hashmap map;
    pz_hashmap_init(&map, 16);

    ASSERT_EQ(0, pz_hashmap_count(&map));
    ASSERT(!pz_hashmap_has(&map, "key1"));

    pz_hashmap_set(&map, "key1", (void *)100);
    pz_hashmap_set(&map, "key2", (void *)200);
    pz_hashmap_set(&map, "key3", (void *)300);

    ASSERT_EQ(3, pz_hashmap_count(&map));
    ASSERT(pz_hashmap_has(&map, "key1"));
    ASSERT(pz_hashmap_has(&map, "key2"));
    ASSERT(pz_hashmap_has(&map, "key3"));
    ASSERT(!pz_hashmap_has(&map, "key4"));

    ASSERT_EQ(100, (size_t)pz_hashmap_get(&map, "key1"));
    ASSERT_EQ(200, (size_t)pz_hashmap_get(&map, "key2"));
    ASSERT_EQ(300, (size_t)pz_hashmap_get(&map, "key3"));
    ASSERT_NULL(pz_hashmap_get(&map, "key4"));

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hashmap_overwrite)
{
    pz_mem_init();

    pz_hashmap map;
    pz_hashmap_init(&map, 16);

    pz_hashmap_set(&map, "key", (void *)100);
    ASSERT_EQ(100, (size_t)pz_hashmap_get(&map, "key"));
    ASSERT_EQ(1, pz_hashmap_count(&map));

    pz_hashmap_set(&map, "key", (void *)200);
    ASSERT_EQ(200, (size_t)pz_hashmap_get(&map, "key"));
    ASSERT_EQ(1, pz_hashmap_count(&map)); // Still 1 entry

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hashmap_remove)
{
    pz_mem_init();

    pz_hashmap map;
    pz_hashmap_init(&map, 16);

    pz_hashmap_set(&map, "key1", (void *)100);
    pz_hashmap_set(&map, "key2", (void *)200);
    pz_hashmap_set(&map, "key3", (void *)300);

    void *removed = pz_hashmap_remove(&map, "key2");
    ASSERT_EQ(200, (size_t)removed);
    ASSERT_EQ(2, pz_hashmap_count(&map));
    ASSERT(!pz_hashmap_has(&map, "key2"));

    // Can still find others
    ASSERT(pz_hashmap_has(&map, "key1"));
    ASSERT(pz_hashmap_has(&map, "key3"));

    // Remove non-existent
    removed = pz_hashmap_remove(&map, "nonexistent");
    ASSERT_NULL(removed);

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hashmap_collision)
{
    pz_mem_init();

    // Use small capacity to force collisions
    pz_hashmap map;
    pz_hashmap_init(&map, 8);

    // Insert many items to force collisions
    char key[16];
    for (int i = 0; i < 20; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        pz_hashmap_set(&map, key, (void *)(size_t)(i + 1));
    }

    ASSERT_EQ(20, pz_hashmap_count(&map));

    // Verify all values
    for (int i = 0; i < 20; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        ASSERT_EQ(i + 1, (size_t)pz_hashmap_get(&map, key));
    }

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hashmap_resize)
{
    pz_mem_init();

    pz_hashmap map;
    pz_hashmap_init(&map, 8);

    // Insert enough to trigger resize
    char key[16];
    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        pz_hashmap_set(&map, key, (void *)(size_t)(i * 10));
    }

    ASSERT_EQ(100, pz_hashmap_count(&map));
    ASSERT(map.capacity > 8); // Should have grown

    // Verify all values still correct after resize
    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        ASSERT_EQ(i * 10, (size_t)pz_hashmap_get(&map, key));
    }

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hashmap_clear)
{
    pz_mem_init();

    pz_hashmap map;
    pz_hashmap_init(&map, 16);

    pz_hashmap_set(&map, "key1", (void *)100);
    pz_hashmap_set(&map, "key2", (void *)200);

    pz_hashmap_clear(&map);

    ASSERT_EQ(0, pz_hashmap_count(&map));
    ASSERT(!pz_hashmap_has(&map, "key1"));
    ASSERT(!pz_hashmap_has(&map, "key2"));

    // Can insert again
    pz_hashmap_set(&map, "key3", (void *)300);
    ASSERT_EQ(1, pz_hashmap_count(&map));
    ASSERT_EQ(300, (size_t)pz_hashmap_get(&map, "key3"));

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hashmap_iterate)
{
    pz_mem_init();

    pz_hashmap map;
    pz_hashmap_init(&map, 16);

    pz_hashmap_set(&map, "a", (void *)1);
    pz_hashmap_set(&map, "b", (void *)2);
    pz_hashmap_set(&map, "c", (void *)3);

    int sum = 0;
    int count = 0;
    pz_hashmap_for_each(&map, key, val)
    {
        (void)key;
        sum += (int)(size_t)val;
        count++;
    }

    ASSERT_EQ(3, count);
    ASSERT_EQ(1 + 2 + 3, sum);

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hashmap_remove_and_reinsert)
{
    pz_mem_init();

    pz_hashmap map;
    pz_hashmap_init(&map, 8);

    // Insert and remove repeatedly to test tombstone handling
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 10; i++) {
            char key[16];
            snprintf(key, sizeof(key), "key%d", i);
            pz_hashmap_set(&map, key, (void *)(size_t)(i + round * 100));
        }

        for (int i = 0; i < 10; i += 2) { // Remove even keys
            char key[16];
            snprintf(key, sizeof(key), "key%d", i);
            pz_hashmap_remove(&map, key);
        }
    }

    // Verify odd keys from last round exist
    for (int i = 1; i < 10; i += 2) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        ASSERT(pz_hashmap_has(&map, key));
    }

    pz_hashmap_destroy(&map);
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(hash_function)
{
    // Test that hash function produces different values for different strings
    uint32_t h1 = pz_hash_string("hello");
    uint32_t h2 = pz_hash_string("world");
    uint32_t h3 = pz_hash_string("hello"); // Same as h1

    ASSERT(h1 != h2);
    ASSERT_EQ(h1, h3);

    // Empty string
    uint32_t h_empty = pz_hash_string("");
    ASSERT(h_empty != 0); // Should have some value (FNV offset basis)
}

TEST_MAIN()

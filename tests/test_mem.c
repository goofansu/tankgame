/*
 * Tank Game - Memory System Tests
 */

#include "test_framework.h"
#include "../src/core/pz_mem.h"

TEST(mem_init_shutdown) {
    pz_mem_init();
    ASSERT_EQ(0, pz_mem_get_allocated());
    ASSERT_EQ(0, pz_mem_get_alloc_count());
    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(mem_alloc_free) {
    pz_mem_init();
    
    void* ptr = pz_alloc(100);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(100, pz_mem_get_allocated());
    ASSERT_EQ(1, pz_mem_get_alloc_count());
    
    pz_free(ptr);
    ASSERT_EQ(0, pz_mem_get_allocated());
    ASSERT_EQ(0, pz_mem_get_alloc_count());
    ASSERT(!pz_mem_has_leaks());
    
    pz_mem_shutdown();
}

TEST(mem_calloc_zeroed) {
    pz_mem_init();
    
    int* arr = pz_calloc(10, sizeof(int));
    ASSERT_NOT_NULL(arr);
    
    // Check all elements are zero
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(0, arr[i]);
    }
    
    pz_free(arr);
    ASSERT(!pz_mem_has_leaks());
    
    pz_mem_shutdown();
}

TEST(mem_realloc_grow) {
    pz_mem_init();
    
    int* arr = pz_alloc(4 * sizeof(int));
    ASSERT_NOT_NULL(arr);
    arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4;
    
    arr = pz_realloc(arr, 8 * sizeof(int));
    ASSERT_NOT_NULL(arr);
    
    // Original values preserved
    ASSERT_EQ(1, arr[0]);
    ASSERT_EQ(2, arr[1]);
    ASSERT_EQ(3, arr[2]);
    ASSERT_EQ(4, arr[3]);
    
    pz_free(arr);
    ASSERT(!pz_mem_has_leaks());
    
    pz_mem_shutdown();
}

TEST(mem_realloc_null) {
    pz_mem_init();
    
    // realloc(NULL, size) should act like alloc
    void* ptr = pz_realloc(NULL, 100);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(100, pz_mem_get_allocated());
    
    pz_free(ptr);
    ASSERT(!pz_mem_has_leaks());
    
    pz_mem_shutdown();
}

TEST(mem_free_null) {
    pz_mem_init();
    
    // Should not crash
    pz_free(NULL);
    ASSERT_EQ(0, pz_mem_get_allocated());
    
    pz_mem_shutdown();
}

TEST(mem_multiple_allocs) {
    pz_mem_init();
    
    void* a = pz_alloc(100);
    void* b = pz_alloc(200);
    void* c = pz_alloc(300);
    
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_EQ(600, pz_mem_get_allocated());
    ASSERT_EQ(3, pz_mem_get_alloc_count());
    
    pz_free(b);  // Free middle one
    ASSERT_EQ(400, pz_mem_get_allocated());
    ASSERT_EQ(2, pz_mem_get_alloc_count());
    
    pz_free(a);
    pz_free(c);
    ASSERT(!pz_mem_has_leaks());
    
    pz_mem_shutdown();
}

#ifdef PZ_DEBUG
TEST(mem_leak_detection) {
    pz_mem_init();
    
    void* leaked = pz_alloc(42);
    (void)leaked;  // Intentionally not freed
    
    ASSERT(pz_mem_has_leaks());
    ASSERT_EQ(42, pz_mem_get_allocated());
    
    // Clean up for test
    pz_free(leaked);
    pz_mem_shutdown();
}

TEST(mem_category_tracking) {
    pz_mem_init();
    
    void* general = pz_alloc_tagged(100, PZ_MEM_GENERAL);
    void* render = pz_alloc_tagged(200, PZ_MEM_RENDER);
    void* game = pz_alloc_tagged(300, PZ_MEM_GAME);
    
    ASSERT_EQ(100, pz_mem_get_category_allocated(PZ_MEM_GENERAL));
    ASSERT_EQ(200, pz_mem_get_category_allocated(PZ_MEM_RENDER));
    ASSERT_EQ(300, pz_mem_get_category_allocated(PZ_MEM_GAME));
    ASSERT_EQ(600, pz_mem_get_allocated());
    
    pz_free(general);
    pz_free(render);
    pz_free(game);
    
    ASSERT_EQ(0, pz_mem_get_category_allocated(PZ_MEM_GENERAL));
    ASSERT_EQ(0, pz_mem_get_category_allocated(PZ_MEM_RENDER));
    ASSERT_EQ(0, pz_mem_get_category_allocated(PZ_MEM_GAME));
    
    pz_mem_shutdown();
}
#endif

TEST_MAIN()

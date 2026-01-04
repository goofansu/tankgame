/*
 * Tank Game - Timsort Implementation
 *
 * A hybrid stable sorting algorithm that combines merge sort and insertion
 * sort. Performs well on real-world data that often contains natural runs
 * (already sorted subsequences).
 */

#ifndef PZ_SORT_H
#define PZ_SORT_H

#include <stddef.h>

// Compare function type: returns negative if a < b, zero if equal, positive if
// a > b
typedef int (*pz_compare_fn)(const void *a, const void *b);

// ============================================================================
// Generic Timsort
// ============================================================================

// Sort an array using timsort
// - base: pointer to array
// - count: number of elements
// - elem_size: size of each element in bytes
// - compare: comparison function
void pz_timsort(
    void *base, size_t count, size_t elem_size, pz_compare_fn compare);

// ============================================================================
// Type-specific convenience functions
// ============================================================================

// Sort an array of floats (ascending order)
void pz_sort_floats(float *arr, size_t count);

// Sort an array of ints (ascending order)
void pz_sort_ints(int *arr, size_t count);

// Sort an array of floats with custom comparison
void pz_sort_floats_cmp(float *arr, size_t count, pz_compare_fn compare);

#endif // PZ_SORT_H

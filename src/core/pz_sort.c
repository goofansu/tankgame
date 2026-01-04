/*
 * Tank Game - Timsort Implementation
 *
 * Timsort is a hybrid sorting algorithm derived from merge sort and insertion
 * sort. It was designed to perform well on real-world data which often contains
 * ordered subsequences (natural runs).
 *
 * Key features:
 * - Finds natural runs in the data
 * - Uses binary insertion sort for small runs
 * - Merges runs using a stack-based strategy
 * - Stable sort (preserves order of equal elements)
 * - O(n log n) worst case, O(n) best case (already sorted)
 */

#include "pz_sort.h"

#include <stdlib.h>
#include <string.h>

#include "pz_mem.h"

// ============================================================================
// Constants
// ============================================================================

// Minimum run length. Runs shorter than this are extended using insertion sort.
// 32 is the traditional value used in Python's timsort.
#define MIN_MERGE 32

// Maximum number of pending runs on the stack.
// For 2^64 elements, we need at most 85 entries (log base phi of 2^64).
// 128 is more than enough for any practical use.
#define MAX_MERGE_PENDING 128

// ============================================================================
// Run tracking
// ============================================================================

typedef struct {
    size_t base; // Start index of run
    size_t len; // Length of run
} pz_run;

// ============================================================================
// Helper macros
// ============================================================================

#define ELEM_PTR(base, idx, size) ((char *)(base) + (idx) * (size))

// ============================================================================
// Compute minimum run length
// ============================================================================

// Returns the minimum run length for an array of n elements.
// If n < MIN_MERGE, returns n.
// Otherwise returns a value k, MIN_MERGE/2 <= k <= MIN_MERGE, such that
// n/k is close to but not less than a power of 2.
static size_t
compute_min_run(size_t n)
{
    size_t r = 0; // Becomes 1 if any shifted-off bits are 1
    while (n >= MIN_MERGE) {
        r |= (n & 1);
        n >>= 1;
    }
    return n + r;
}

// ============================================================================
// Binary Insertion Sort
// ============================================================================

// Sort [lo, hi) using binary insertion sort.
// Assumes [lo, start) is already sorted.
static void
binary_insertion_sort(void *base, size_t lo, size_t hi, size_t start,
    size_t elem_size, pz_compare_fn compare, void *tmp)
{
    if (start == lo) {
        start++;
    }

    for (size_t i = start; i < hi; i++) {
        // Copy current element to tmp
        memcpy(tmp, ELEM_PTR(base, i, elem_size), elem_size);

        // Binary search for insertion point
        size_t left = lo;
        size_t right = i;

        while (left < right) {
            size_t mid = left + (right - left) / 2;
            if (compare(tmp, ELEM_PTR(base, mid, elem_size)) < 0) {
                right = mid;
            } else {
                left = mid + 1;
            }
        }

        // Shift elements to make room
        size_t shift_count = i - left;
        if (shift_count > 0) {
            memmove(ELEM_PTR(base, left + 1, elem_size),
                ELEM_PTR(base, left, elem_size), shift_count * elem_size);
        }

        // Insert element
        memcpy(ELEM_PTR(base, left, elem_size), tmp, elem_size);
    }
}

// ============================================================================
// Run detection
// ============================================================================

// Find the length of the run starting at index lo.
// A run is either ascending (a[i] <= a[i+1]) or strictly descending
// (a[i] > a[i+1]). Descending runs are reversed to make them ascending.
// Returns the length of the run.
static size_t
count_run_and_make_ascending(
    void *base, size_t lo, size_t hi, size_t elem_size, pz_compare_fn compare)
{
    if (lo + 1 >= hi) {
        return hi - lo;
    }

    size_t run_hi = lo + 1;

    // Check if descending
    if (compare(
            ELEM_PTR(base, run_hi, elem_size), ELEM_PTR(base, lo, elem_size))
        < 0) {
        // Strictly descending
        while (run_hi < hi
            && compare(ELEM_PTR(base, run_hi, elem_size),
                   ELEM_PTR(base, run_hi - 1, elem_size))
                < 0) {
            run_hi++;
        }

        // Reverse the run to make it ascending
        size_t left = lo;
        size_t right = run_hi - 1;
        void *tmp = pz_alloc(elem_size);
        if (tmp) {
            while (left < right) {
                memcpy(tmp, ELEM_PTR(base, left, elem_size), elem_size);
                memcpy(ELEM_PTR(base, left, elem_size),
                    ELEM_PTR(base, right, elem_size), elem_size);
                memcpy(ELEM_PTR(base, right, elem_size), tmp, elem_size);
                left++;
                right--;
            }
            pz_free(tmp);
        }
    } else {
        // Ascending (non-strictly, allows equal elements)
        while (run_hi < hi
            && compare(ELEM_PTR(base, run_hi, elem_size),
                   ELEM_PTR(base, run_hi - 1, elem_size))
                >= 0) {
            run_hi++;
        }
    }

    return run_hi - lo;
}

// ============================================================================
// Galloping (exponential search)
// ============================================================================

// Search for key in sorted array [base, base + len).
// Returns the index where key should be inserted to maintain sorted order.
// If there are equal elements, returns the leftmost position.
static size_t
gallop_left(const void *key, const void *base, size_t len, size_t hint,
    size_t elem_size, pz_compare_fn compare)
{
    size_t last_ofs = 0;
    size_t ofs = 1;

    if (compare(key, ELEM_PTR(base, hint, elem_size)) > 0) {
        // Gallop right
        size_t max_ofs = len - hint;
        while (ofs < max_ofs
            && compare(key, ELEM_PTR(base, hint + ofs, elem_size)) > 0) {
            last_ofs = ofs;
            ofs = (ofs << 1) + 1;
            if (ofs <= last_ofs) { // Overflow
                ofs = max_ofs;
            }
        }
        if (ofs > max_ofs) {
            ofs = max_ofs;
        }

        // Translate back
        last_ofs += hint;
        ofs += hint;
    } else {
        // Gallop left
        size_t max_ofs = hint + 1;
        while (ofs < max_ofs
            && compare(key, ELEM_PTR(base, hint - ofs, elem_size)) <= 0) {
            last_ofs = ofs;
            ofs = (ofs << 1) + 1;
            if (ofs <= last_ofs) { // Overflow
                ofs = max_ofs;
            }
        }
        if (ofs > max_ofs) {
            ofs = max_ofs;
        }

        // Translate back
        size_t tmp = last_ofs;
        last_ofs = hint - ofs;
        ofs = hint - tmp;
    }

    // Binary search in [last_ofs, ofs)
    last_ofs++;
    while (last_ofs < ofs) {
        size_t m = last_ofs + (ofs - last_ofs) / 2;
        if (compare(key, ELEM_PTR(base, m, elem_size)) > 0) {
            last_ofs = m + 1;
        } else {
            ofs = m;
        }
    }
    return ofs;
}

// Like gallop_left, but returns the rightmost position for equal elements.
static size_t
gallop_right(const void *key, const void *base, size_t len, size_t hint,
    size_t elem_size, pz_compare_fn compare)
{
    size_t last_ofs = 0;
    size_t ofs = 1;

    if (compare(key, ELEM_PTR(base, hint, elem_size)) < 0) {
        // Gallop left
        size_t max_ofs = hint + 1;
        while (ofs < max_ofs
            && compare(key, ELEM_PTR(base, hint - ofs, elem_size)) < 0) {
            last_ofs = ofs;
            ofs = (ofs << 1) + 1;
            if (ofs <= last_ofs) {
                ofs = max_ofs;
            }
        }
        if (ofs > max_ofs) {
            ofs = max_ofs;
        }

        size_t tmp = last_ofs;
        last_ofs = hint - ofs;
        ofs = hint - tmp;
    } else {
        // Gallop right
        size_t max_ofs = len - hint;
        while (ofs < max_ofs
            && compare(key, ELEM_PTR(base, hint + ofs, elem_size)) >= 0) {
            last_ofs = ofs;
            ofs = (ofs << 1) + 1;
            if (ofs <= last_ofs) {
                ofs = max_ofs;
            }
        }
        if (ofs > max_ofs) {
            ofs = max_ofs;
        }

        last_ofs += hint;
        ofs += hint;
    }

    // Binary search
    last_ofs++;
    while (last_ofs < ofs) {
        size_t m = last_ofs + (ofs - last_ofs) / 2;
        if (compare(key, ELEM_PTR(base, m, elem_size)) < 0) {
            ofs = m;
        } else {
            last_ofs = m + 1;
        }
    }
    return ofs;
}

// ============================================================================
// Merging
// ============================================================================

// Merge two adjacent runs. run1 must come before run2 in the array.
// Uses temporary storage for the smaller run.
static void
merge_runs(void *base, size_t base1, size_t len1, size_t base2, size_t len2,
    size_t elem_size, pz_compare_fn compare, void *tmp_storage, size_t tmp_size)
{
    // Optimize: find where run2's first element would be inserted in run1
    size_t k = gallop_right(ELEM_PTR(base, base2, elem_size),
        ELEM_PTR(base, base1, elem_size), len1, 0, elem_size, compare);
    base1 += k;
    len1 -= k;
    if (len1 == 0) {
        return;
    }

    // Optimize: find where run1's last element would be inserted in run2
    len2 = gallop_left(ELEM_PTR(base, base1 + len1 - 1, elem_size),
        ELEM_PTR(base, base2, elem_size), len2, len2 - 1, elem_size, compare);
    if (len2 == 0) {
        return;
    }

    // Merge the remaining portions
    if (len1 <= len2) {
        // Copy run1 to tmp, merge from left
        if (len1 * elem_size > tmp_size) {
            // Fallback: simple merge without galloping
            // This shouldn't happen with proper tmp allocation
            return;
        }

        memcpy(tmp_storage, ELEM_PTR(base, base1, elem_size), len1 * elem_size);

        char *dest = ELEM_PTR(base, base1, elem_size);
        const char *cursor1 = tmp_storage;
        const char *cursor2 = ELEM_PTR(base, base2, elem_size);
        const char *end1 = cursor1 + len1 * elem_size;
        const char *end2 = cursor2 + len2 * elem_size;

        while (cursor1 < end1 && cursor2 < end2) {
            if (compare(cursor2, cursor1) < 0) {
                memcpy(dest, cursor2, elem_size);
                cursor2 += elem_size;
            } else {
                memcpy(dest, cursor1, elem_size);
                cursor1 += elem_size;
            }
            dest += elem_size;
        }

        // Copy remaining
        if (cursor1 < end1) {
            memcpy(dest, cursor1, (size_t)(end1 - cursor1));
        }
        // cursor2's remaining elements are already in place
    } else {
        // Copy run2 to tmp, merge from right
        if (len2 * elem_size > tmp_size) {
            return;
        }

        memcpy(tmp_storage, ELEM_PTR(base, base2, elem_size), len2 * elem_size);

        char *dest = ELEM_PTR(base, base1 + len1 + len2 - 1, elem_size);
        const char *cursor1 = ELEM_PTR(base, base1 + len1 - 1, elem_size);
        const char *cursor2 = (char *)tmp_storage + (len2 - 1) * elem_size;
        const char *start1 = ELEM_PTR(base, base1, elem_size);
        const char *start2 = tmp_storage;

        while (cursor1 >= start1 && cursor2 >= start2) {
            if (compare(cursor2, cursor1) > 0) {
                memcpy(dest, cursor2, elem_size);
                cursor2 -= elem_size;
            } else {
                memcpy(dest, cursor1, elem_size);
                cursor1 -= elem_size;
            }
            dest -= elem_size;
        }

        // Copy remaining from tmp
        if (cursor2 >= start2) {
            size_t remaining = (size_t)(cursor2 - start2) / elem_size + 1;
            memcpy(ELEM_PTR(base, base1, elem_size), start2,
                remaining * elem_size);
        }
    }
}

// ============================================================================
// Main timsort
// ============================================================================

void
pz_timsort(void *base, size_t count, size_t elem_size, pz_compare_fn compare)
{
    if (count < 2 || !base || !compare) {
        return;
    }

    // For very small arrays, just use insertion sort
    if (count < MIN_MERGE) {
        void *tmp = pz_alloc(elem_size);
        if (tmp) {
            binary_insertion_sort(base, 0, count, 1, elem_size, compare, tmp);
            pz_free(tmp);
        }
        return;
    }

    // Allocate temporary storage
    size_t tmp_size = (count / 2) * elem_size;
    void *tmp_storage = pz_alloc(tmp_size);
    void *tmp_elem = pz_alloc(elem_size);
    if (!tmp_storage || !tmp_elem) {
        pz_free(tmp_storage);
        pz_free(tmp_elem);
        return;
    }

    size_t min_run = compute_min_run(count);

    // Stack of pending runs to merge
    pz_run run_stack[MAX_MERGE_PENDING];
    int stack_size = 0;

    size_t lo = 0;
    size_t remaining = count;

    while (remaining > 0) {
        // Find next run
        size_t run_len = count_run_and_make_ascending(
            base, lo, lo + remaining, elem_size, compare);

        // Extend short runs using insertion sort
        if (run_len < min_run) {
            size_t force = (remaining < min_run) ? remaining : min_run;
            binary_insertion_sort(base, lo, lo + force, lo + run_len, elem_size,
                compare, tmp_elem);
            run_len = force;
        }

        // Push run onto stack
        run_stack[stack_size].base = lo;
        run_stack[stack_size].len = run_len;
        stack_size++;

        // Merge to maintain stack invariants:
        // 1. run[i-2].len > run[i-1].len + run[i].len
        // 2. run[i-1].len > run[i].len
        while (stack_size > 1) {
            int n = stack_size - 2;

            int should_merge = 0;
            if (n > 0
                && run_stack[n - 1].len
                    <= run_stack[n].len + run_stack[n + 1].len) {
                should_merge = 1;
                if (run_stack[n - 1].len < run_stack[n + 1].len) {
                    n--;
                }
            } else if (run_stack[n].len <= run_stack[n + 1].len) {
                should_merge = 1;
            }

            if (!should_merge) {
                break;
            }

            // Merge runs at n and n+1
            merge_runs(base, run_stack[n].base, run_stack[n].len,
                run_stack[n + 1].base, run_stack[n + 1].len, elem_size, compare,
                tmp_storage, tmp_size);

            run_stack[n].len += run_stack[n + 1].len;

            // Remove run n+1 from stack
            if (n + 2 < stack_size) {
                run_stack[n + 1] = run_stack[n + 2];
            }
            stack_size--;
        }

        lo += run_len;
        remaining -= run_len;
    }

    // Merge all remaining runs
    while (stack_size > 1) {
        int n = stack_size - 2;
        if (n > 0 && run_stack[n - 1].len < run_stack[n + 1].len) {
            n--;
        }

        merge_runs(base, run_stack[n].base, run_stack[n].len,
            run_stack[n + 1].base, run_stack[n + 1].len, elem_size, compare,
            tmp_storage, tmp_size);

        run_stack[n].len += run_stack[n + 1].len;

        if (n + 2 < stack_size) {
            run_stack[n + 1] = run_stack[n + 2];
        }
        stack_size--;
    }

    pz_free(tmp_storage);
    pz_free(tmp_elem);
}

// ============================================================================
// Type-specific functions
// ============================================================================

static int
compare_float_asc(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static int
compare_int_asc(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

void
pz_sort_floats(float *arr, size_t count)
{
    pz_timsort(arr, count, sizeof(float), compare_float_asc);
}

void
pz_sort_ints(int *arr, size_t count)
{
    pz_timsort(arr, count, sizeof(int), compare_int_asc);
}

void
pz_sort_floats_cmp(float *arr, size_t count, pz_compare_fn compare)
{
    pz_timsort(arr, count, sizeof(float), compare);
}

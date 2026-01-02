/*
 * Tank Game - String Utilities
 *
 * Provides common string operations that allocate memory via pz_mem.
 */

#ifndef PZ_STR_H
#define PZ_STR_H

#include <stdbool.h>
#include <stddef.h>

// Duplicate a string (caller must pz_free)
char *pz_str_dup(const char *str);

// Duplicate at most n characters (caller must pz_free)
char *pz_str_ndup(const char *str, size_t n);

// Format string (allocating sprintf, caller must pz_free)
char *pz_str_fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Split string by delimiter (returns NULL-terminated array of strings)
// All strings and the array itself must be freed with pz_str_split_free
char **pz_str_split(const char *str, char delim, size_t *out_count);

// Free result of pz_str_split
void pz_str_split_free(char **parts);

// Trim whitespace from both ends (returns new string, caller must pz_free)
char *pz_str_trim(const char *str);

// Trim whitespace from left (returns new string, caller must pz_free)
char *pz_str_ltrim(const char *str);

// Trim whitespace from right (returns new string, caller must pz_free)
char *pz_str_rtrim(const char *str);

// Check if string starts with prefix
bool pz_str_starts_with(const char *str, const char *prefix);

// Check if string ends with suffix
bool pz_str_ends_with(const char *str, const char *suffix);

// Parse integer (returns true on success)
bool pz_str_to_int(const char *str, int *out);

// Parse long (returns true on success)
bool pz_str_to_long(const char *str, long *out);

// Parse float (returns true on success)
bool pz_str_to_float(const char *str, float *out);

// Parse double (returns true on success)
bool pz_str_to_double(const char *str, double *out);

// Check if string is empty or NULL
bool pz_str_empty(const char *str);

// Compare strings (NULL-safe, NULLs sort first)
int pz_str_cmp(const char *a, const char *b);

// Case-insensitive compare
int pz_str_casecmp(const char *a, const char *b);

// Join strings with separator (caller must pz_free result)
char *pz_str_join(const char **strings, size_t count, const char *sep);

// Replace all occurrences of 'old' with 'new' (caller must pz_free result)
char *pz_str_replace(const char *str, const char *old_str, const char *new_str);

#endif // PZ_STR_H

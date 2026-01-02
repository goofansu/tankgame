/*
 * Tank Game - String Utilities Implementation
 */

#include "pz_str.h"
#include "pz_mem.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *
pz_str_dup(const char *str)
{
    if (str == NULL) {
        return NULL;
    }
    size_t len = strlen(str);
    char *dup = pz_alloc(len + 1);
    if (dup) {
        memcpy(dup, str, len + 1);
    }
    return dup;
}

char *
pz_str_ndup(const char *str, size_t n)
{
    if (str == NULL) {
        return NULL;
    }

    // Find actual length (min of n and strlen)
    size_t len = 0;
    while (len < n && str[len] != '\0') {
        len++;
    }

    char *dup = pz_alloc(len + 1);
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

char *
pz_str_fmt(const char *fmt, ...)
{
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    // Get required size
    int size = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);

    if (size < 0) {
        va_end(args2);
        return NULL;
    }

    char *str = pz_alloc((size_t)size + 1);
    if (str) {
        vsnprintf(str, (size_t)size + 1, fmt, args2);
    }

    va_end(args2);
    return str;
}

char **
pz_str_split(const char *str, char delim, size_t *out_count)
{
    if (str == NULL) {
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    // Count parts
    size_t count = 1;
    const char *p = str;
    while (*p) {
        if (*p == delim) {
            count++;
        }
        p++;
    }

    // Allocate array (count + 1 for NULL terminator)
    char **parts = pz_alloc((count + 1) * sizeof(char *));
    if (!parts) {
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    // Split
    size_t idx = 0;
    const char *start = str;
    p = str;

    while (*p) {
        if (*p == delim) {
            parts[idx] = pz_str_ndup(start, (size_t)(p - start));
            idx++;
            start = p + 1;
        }
        p++;
    }

    // Last part
    parts[idx] = pz_str_dup(start);
    idx++;
    parts[idx] = NULL;

    if (out_count)
        *out_count = count;
    return parts;
}

void
pz_str_split_free(char **parts)
{
    if (parts == NULL)
        return;

    for (char **p = parts; *p != NULL; p++) {
        pz_free(*p);
    }
    pz_free(parts);
}

static bool
is_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v'
        || c == '\f';
}

char *
pz_str_ltrim(const char *str)
{
    if (str == NULL)
        return NULL;

    while (*str && is_whitespace(*str)) {
        str++;
    }
    return pz_str_dup(str);
}

char *
pz_str_rtrim(const char *str)
{
    if (str == NULL)
        return NULL;

    size_t len = strlen(str);
    while (len > 0 && is_whitespace(str[len - 1])) {
        len--;
    }
    return pz_str_ndup(str, len);
}

char *
pz_str_trim(const char *str)
{
    if (str == NULL)
        return NULL;

    // Skip leading whitespace
    while (*str && is_whitespace(*str)) {
        str++;
    }

    // Find end
    size_t len = strlen(str);
    while (len > 0 && is_whitespace(str[len - 1])) {
        len--;
    }

    return pz_str_ndup(str, len);
}

bool
pz_str_starts_with(const char *str, const char *prefix)
{
    if (str == NULL || prefix == NULL)
        return false;

    while (*prefix) {
        if (*str != *prefix) {
            return false;
        }
        str++;
        prefix++;
    }
    return true;
}

bool
pz_str_ends_with(const char *str, const char *suffix)
{
    if (str == NULL || suffix == NULL)
        return false;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len)
        return false;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

bool
pz_str_to_int(const char *str, int *out)
{
    if (str == NULL || *str == '\0')
        return false;

    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    // Check for errors
    if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
        return false;
    }

    // Check that we consumed the whole string
    if (*endptr != '\0') {
        return false;
    }

    *out = (int)val;
    return true;
}

bool
pz_str_to_long(const char *str, long *out)
{
    if (str == NULL || *str == '\0')
        return false;

    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    if (errno == ERANGE) {
        return false;
    }

    if (*endptr != '\0') {
        return false;
    }

    *out = val;
    return true;
}

bool
pz_str_to_float(const char *str, float *out)
{
    if (str == NULL || *str == '\0')
        return false;

    char *endptr;
    errno = 0;
    float val = strtof(str, &endptr);

    if (errno == ERANGE) {
        return false;
    }

    if (*endptr != '\0') {
        return false;
    }

    *out = val;
    return true;
}

bool
pz_str_to_double(const char *str, double *out)
{
    if (str == NULL || *str == '\0')
        return false;

    char *endptr;
    errno = 0;
    double val = strtod(str, &endptr);

    if (errno == ERANGE) {
        return false;
    }

    if (*endptr != '\0') {
        return false;
    }

    *out = val;
    return true;
}

bool
pz_str_empty(const char *str)
{
    return str == NULL || *str == '\0';
}

int
pz_str_cmp(const char *a, const char *b)
{
    if (a == b)
        return 0;
    if (a == NULL)
        return -1;
    if (b == NULL)
        return 1;
    return strcmp(a, b);
}

int
pz_str_casecmp(const char *a, const char *b)
{
    if (a == b)
        return 0;
    if (a == NULL)
        return -1;
    if (b == NULL)
        return 1;

    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) {
            return ca - cb;
        }
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

char *
pz_str_join(const char **strings, size_t count, const char *sep)
{
    if (strings == NULL || count == 0) {
        return pz_str_dup("");
    }

    if (sep == NULL) {
        sep = "";
    }

    // Calculate total length
    size_t sep_len = strlen(sep);
    size_t total_len = 0;

    for (size_t i = 0; i < count; i++) {
        if (strings[i]) {
            total_len += strlen(strings[i]);
        }
        if (i < count - 1) {
            total_len += sep_len;
        }
    }

    char *result = pz_alloc(total_len + 1);
    if (!result)
        return NULL;

    char *p = result;
    for (size_t i = 0; i < count; i++) {
        if (strings[i]) {
            size_t len = strlen(strings[i]);
            memcpy(p, strings[i], len);
            p += len;
        }
        if (i < count - 1) {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
    }
    *p = '\0';

    return result;
}

char *
pz_str_replace(const char *str, const char *old_str, const char *new_str)
{
    if (str == NULL)
        return NULL;
    if (old_str == NULL || *old_str == '\0')
        return pz_str_dup(str);
    if (new_str == NULL)
        new_str = "";

    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);

    // Count occurrences
    size_t count = 0;
    const char *p = str;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }

    if (count == 0) {
        return pz_str_dup(str);
    }

    // Calculate new length
    size_t str_len = strlen(str);
    size_t result_len = str_len + count * (new_len - old_len);

    char *result = pz_alloc(result_len + 1);
    if (!result)
        return NULL;

    char *dst = result;
    p = str;
    const char *next;

    while ((next = strstr(p, old_str)) != NULL) {
        // Copy before match
        size_t prefix_len = (size_t)(next - p);
        memcpy(dst, p, prefix_len);
        dst += prefix_len;

        // Copy replacement
        memcpy(dst, new_str, new_len);
        dst += new_len;

        p = next + old_len;
    }

    // Copy remainder
    strcpy(dst, p);

    return result;
}

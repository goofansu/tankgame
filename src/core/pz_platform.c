/*
 * Tank Game - Platform Layer Implementation
 */

#include "pz_platform.h"
#include "pz_mem.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Platform-specific includes
#ifdef __EMSCRIPTEN__
#    include <emscripten/emscripten.h>
#elif defined(__APPLE__)
#    include <mach/mach_time.h>
#else
#    include <time.h>
#endif

/* ============================================================================
 * High-Precision Timer
 * ============================================================================
 */

#ifdef __EMSCRIPTEN__
static double s_time_start;
#elif defined(__APPLE__)
// macOS uses mach_absolute_time
static mach_timebase_info_data_t s_timebase_info;
static uint64_t s_time_start;
#else
// Linux/other POSIX uses clock_gettime
static struct timespec s_time_start;
#endif

void
pz_time_init(void)
{
#ifdef __EMSCRIPTEN__
    s_time_start = emscripten_get_now();
#elif defined(__APPLE__)
    mach_timebase_info(&s_timebase_info);
    s_time_start = mach_absolute_time();
#else
    clock_gettime(CLOCK_MONOTONIC, &s_time_start);
#endif
}

double
pz_time_now(void)
{
#ifdef __EMSCRIPTEN__
    return (emscripten_get_now() - s_time_start) / 1000.0;
#elif defined(__APPLE__)
    uint64_t elapsed = mach_absolute_time() - s_time_start;
    uint64_t ns = elapsed * s_timebase_info.numer / s_timebase_info.denom;
    return (double)ns / 1e9;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - s_time_start.tv_sec)
        + (now.tv_nsec - s_time_start.tv_nsec) / 1e9;
    return elapsed;
#endif
}

uint64_t
pz_time_now_ms(void)
{
#ifdef __EMSCRIPTEN__
    return (uint64_t)(emscripten_get_now() - s_time_start);
#elif defined(__APPLE__)
    uint64_t elapsed = mach_absolute_time() - s_time_start;
    uint64_t ns = elapsed * s_timebase_info.numer / s_timebase_info.denom;
    return ns / 1000000;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t ms = (uint64_t)(now.tv_sec - s_time_start.tv_sec) * 1000;
    ms += (uint64_t)(now.tv_nsec - s_time_start.tv_nsec) / 1000000;
    return ms;
#endif
}

uint64_t
pz_time_now_us(void)
{
#ifdef __EMSCRIPTEN__
    return (uint64_t)((emscripten_get_now() - s_time_start) * 1000.0);
#elif defined(__APPLE__)
    uint64_t elapsed = mach_absolute_time() - s_time_start;
    uint64_t ns = elapsed * s_timebase_info.numer / s_timebase_info.denom;
    return ns / 1000;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t us = (uint64_t)(now.tv_sec - s_time_start.tv_sec) * 1000000;
    us += (uint64_t)(now.tv_nsec - s_time_start.tv_nsec) / 1000;
    return us;
#endif
}

void
pz_time_sleep_ms(uint32_t ms)
{
#ifdef __EMSCRIPTEN__
    (void)ms;
#else
    usleep(ms * 1000);
#endif
}

/* ============================================================================
 * File Operations
 * ============================================================================
 */

char *
pz_file_read(const char *path, size_t *out_size)
{
    if (out_size)
        *out_size = 0;

    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    char *data = pz_alloc((size_t)size + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        pz_free(data);
        return NULL;
    }

    data[size] = '\0'; // Null-terminate for convenience

    if (out_size)
        *out_size = (size_t)size;
    return data;
}

char *
pz_file_read_text(const char *path)
{
    return pz_file_read(path, NULL);
}

bool
pz_file_write(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    return written == size;
}

bool
pz_file_write_text(const char *path, const char *text)
{
    if (!text)
        return false;
    return pz_file_write(path, text, strlen(text));
}

bool
pz_file_append(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "ab");
    if (!f)
        return false;

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    return written == size;
}

bool
pz_file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int64_t
pz_file_mtime(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return (int64_t)st.st_mtime;
}

int64_t
pz_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    return (int64_t)st.st_size;
}

bool
pz_file_delete(const char *path)
{
    return unlink(path) == 0;
}

/* ============================================================================
 * Directory Operations
 * ============================================================================
 */

bool
pz_dir_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool
pz_dir_create(const char *path)
{
    // Try to create directory
    if (mkdir(path, 0755) == 0)
        return true;

    // If exists, that's fine
    if (errno == EEXIST && pz_dir_exists(path))
        return true;

    // If parent doesn't exist, create it recursively
    if (errno == ENOENT) {
        char *parent = pz_path_dirname(path);
        if (parent) {
            bool ok = pz_dir_create(parent);
            pz_free(parent);
            if (ok) {
                return mkdir(path, 0755) == 0;
            }
        }
    }

    return false;
}

char *
pz_dir_cwd(void)
{
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL)
        return NULL;

    size_t len = strlen(buf);
    char *result = pz_alloc(len + 1);
    if (result) {
        memcpy(result, buf, len + 1);
    }
    return result;
}

/* ============================================================================
 * Path Operations
 * ============================================================================
 */

char *
pz_path_join(const char *a, const char *b)
{
    if (!a || !*a)
        return b ? pz_alloc(strlen(b) + 1) : NULL;
    if (!b || !*b)
        return pz_alloc(strlen(a) + 1);

    size_t a_len = strlen(a);
    size_t b_len = strlen(b);

    // Check if a ends with separator
    bool has_sep = (a[a_len - 1] == '/');

    // Check if b starts with separator
    if (b[0] == '/') {
        b++;
        b_len--;
    }

    size_t total = a_len + b_len + (has_sep ? 0 : 1) + 1;
    char *result = pz_alloc(total);
    if (!result)
        return NULL;

    memcpy(result, a, a_len);
    if (!has_sep) {
        result[a_len] = '/';
        a_len++;
    }
    memcpy(result + a_len, b, b_len + 1);

    return result;
}

char *
pz_path_filename(const char *path)
{
    if (!path)
        return NULL;

    const char *last_sep = strrchr(path, '/');
    const char *filename = last_sep ? last_sep + 1 : path;

    size_t len = strlen(filename);
    char *result = pz_alloc(len + 1);
    if (result) {
        memcpy(result, filename, len + 1);
    }
    return result;
}

char *
pz_path_dirname(const char *path)
{
    if (!path)
        return NULL;

    const char *last_sep = strrchr(path, '/');
    if (!last_sep) {
        // No separator - return "."
        char *result = pz_alloc(2);
        if (result) {
            result[0] = '.';
            result[1] = '\0';
        }
        return result;
    }

    // If separator is at start, return "/"
    if (last_sep == path) {
        char *result = pz_alloc(2);
        if (result) {
            result[0] = '/';
            result[1] = '\0';
        }
        return result;
    }

    size_t len = (size_t)(last_sep - path);
    char *result = pz_alloc(len + 1);
    if (result) {
        memcpy(result, path, len);
        result[len] = '\0';
    }
    return result;
}

char *
pz_path_extension(const char *path)
{
    if (!path)
        return NULL;

    // Get filename first
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    // Find last dot in filename
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        // No extension
        char *result = pz_alloc(1);
        if (result)
            result[0] = '\0';
        return result;
    }

    size_t len = strlen(dot + 1);
    char *result = pz_alloc(len + 1);
    if (result) {
        memcpy(result, dot + 1, len + 1);
    }
    return result;
}

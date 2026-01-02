/*
 * Tank Game - Platform Layer Tests
 */

#include "../src/core/pz_mem.h"
#include "../src/core/pz_platform.h"
#include "test_framework.h"
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * Timer Tests
 * ============================================================================
 */

TEST(time_init)
{
    pz_time_init();
    // Should not crash
    ASSERT(1);
}

TEST(time_now)
{
    pz_time_init();

    double t1 = pz_time_now();
    pz_time_sleep_ms(10);
    double t2 = pz_time_now();

    // Should have passed at least 10ms (0.01s)
    ASSERT(t2 > t1);
    ASSERT(t2 - t1 >= 0.009); // Allow small timing variance
    ASSERT(t2 - t1 < 0.1); // But not too much
}

TEST(time_now_ms)
{
    pz_time_init();

    uint64_t t1 = pz_time_now_ms();
    pz_time_sleep_ms(20);
    uint64_t t2 = pz_time_now_ms();

    ASSERT(t2 > t1);
    ASSERT(t2 - t1 >= 18); // Allow small variance
    ASSERT(t2 - t1 < 100);
}

TEST(time_now_us)
{
    pz_time_init();

    uint64_t t1 = pz_time_now_us();
    pz_time_sleep_ms(5);
    uint64_t t2 = pz_time_now_us();

    ASSERT(t2 > t1);
    ASSERT(t2 - t1 >= 4000); // At least 4ms in microseconds
    ASSERT(t2 - t1 < 100000); // Less than 100ms
}

/* ============================================================================
 * File Tests
 * ============================================================================
 */

TEST(file_write_read)
{
    pz_mem_init();

    const char *path = "/tmp/pz_test_file.txt";
    const char *content = "Hello, World!";

    // Write
    ASSERT(pz_file_write_text(path, content));
    ASSERT(pz_file_exists(path));

    // Read
    char *read_content = pz_file_read_text(path);
    ASSERT_NOT_NULL(read_content);
    ASSERT_STR_EQ(content, read_content);
    pz_free(read_content);

    // Read with size
    size_t size = 0;
    read_content = pz_file_read(path, &size);
    ASSERT_NOT_NULL(read_content);
    ASSERT_EQ(strlen(content), size);
    pz_free(read_content);

    // Clean up
    ASSERT(pz_file_delete(path));
    ASSERT(!pz_file_exists(path));

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(file_write_binary)
{
    pz_mem_init();

    const char *path = "/tmp/pz_test_binary.bin";
    uint8_t data[] = { 0x00, 0x01, 0x02, 0xFF, 0xFE, 0x00, 0x03 };
    size_t data_size = sizeof(data);

    ASSERT(pz_file_write(path, data, data_size));

    size_t read_size;
    char *read_data = pz_file_read(path, &read_size);
    ASSERT_NOT_NULL(read_data);
    ASSERT_EQ(data_size, read_size);
    ASSERT(memcmp(data, read_data, data_size) == 0);

    pz_free(read_data);
    pz_file_delete(path);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(file_append)
{
    pz_mem_init();

    const char *path = "/tmp/pz_test_append.txt";

    pz_file_delete(path); // Clean start

    ASSERT(pz_file_write_text(path, "Hello"));
    ASSERT(pz_file_append(path, ", ", 2));
    ASSERT(pz_file_append(path, "World!", 6));

    char *content = pz_file_read_text(path);
    ASSERT_NOT_NULL(content);
    ASSERT_STR_EQ("Hello, World!", content);
    pz_free(content);

    pz_file_delete(path);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(file_mtime)
{
    pz_mem_init();

    const char *path = "/tmp/pz_test_mtime.txt";
    pz_file_write_text(path, "test");

    int64_t mtime1 = pz_file_mtime(path);
    ASSERT(mtime1 > 0);

    pz_time_sleep_ms(1100); // Wait over 1 second

    pz_file_write_text(path, "modified");
    int64_t mtime2 = pz_file_mtime(path);

    ASSERT(mtime2 >= mtime1); // Should be same or later

    pz_file_delete(path);

    // Non-existent file
    ASSERT_EQ(0, pz_file_mtime("/tmp/nonexistent_12345.txt"));

    pz_mem_shutdown();
}

TEST(file_size)
{
    pz_mem_init();

    const char *path = "/tmp/pz_test_size.txt";
    const char *content = "0123456789"; // 10 bytes

    pz_file_write_text(path, content);

    int64_t size = pz_file_size(path);
    ASSERT_EQ(10, size);

    pz_file_delete(path);

    // Non-existent file
    ASSERT_EQ(-1, pz_file_size("/tmp/nonexistent_12345.txt"));

    pz_mem_shutdown();
}

TEST(file_not_exists)
{
    pz_mem_init();

    ASSERT(!pz_file_exists("/tmp/definitely_not_exists_12345.txt"));

    char *content = pz_file_read_text("/tmp/definitely_not_exists_12345.txt");
    ASSERT_NULL(content);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

/* ============================================================================
 * Directory Tests
 * ============================================================================
 */

TEST(dir_exists)
{
    ASSERT(pz_dir_exists("/tmp"));
    ASSERT(!pz_dir_exists("/tmp/definitely_not_exists_dir_12345"));
}

TEST(dir_create)
{
    pz_mem_init();

    const char *path = "/tmp/pz_test_dir";
    const char *nested = "/tmp/pz_test_dir/nested/deep";

    // Clean up from previous runs
    rmdir("/tmp/pz_test_dir/nested/deep");
    rmdir("/tmp/pz_test_dir/nested");
    rmdir("/tmp/pz_test_dir");

    ASSERT(pz_dir_create(path));
    ASSERT(pz_dir_exists(path));

    // Nested
    ASSERT(pz_dir_create(nested));
    ASSERT(pz_dir_exists(nested));

    // Clean up
    rmdir("/tmp/pz_test_dir/nested/deep");
    rmdir("/tmp/pz_test_dir/nested");
    rmdir("/tmp/pz_test_dir");

    pz_mem_shutdown();
}

TEST(dir_cwd)
{
    pz_mem_init();

    char *cwd = pz_dir_cwd();
    ASSERT_NOT_NULL(cwd);
    ASSERT(strlen(cwd) > 0);
    ASSERT(cwd[0] == '/'); // Should be absolute path

    pz_free(cwd);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

/* ============================================================================
 * Path Tests
 * ============================================================================
 */

TEST(path_join)
{
    pz_mem_init();

    char *p = pz_path_join("/home/user", "file.txt");
    ASSERT_STR_EQ("/home/user/file.txt", p);
    pz_free(p);

    // Already has separator
    p = pz_path_join("/home/user/", "file.txt");
    ASSERT_STR_EQ("/home/user/file.txt", p);
    pz_free(p);

    // Second starts with separator
    p = pz_path_join("/home/user", "/file.txt");
    ASSERT_STR_EQ("/home/user/file.txt", p);
    pz_free(p);

    // Both have separator
    p = pz_path_join("/home/user/", "/file.txt");
    ASSERT_STR_EQ("/home/user/file.txt", p);
    pz_free(p);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(path_filename)
{
    pz_mem_init();

    char *f = pz_path_filename("/home/user/file.txt");
    ASSERT_STR_EQ("file.txt", f);
    pz_free(f);

    f = pz_path_filename("file.txt");
    ASSERT_STR_EQ("file.txt", f);
    pz_free(f);

    f = pz_path_filename("/home/user/");
    ASSERT_STR_EQ("", f);
    pz_free(f);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(path_dirname)
{
    pz_mem_init();

    char *d = pz_path_dirname("/home/user/file.txt");
    ASSERT_STR_EQ("/home/user", d);
    pz_free(d);

    d = pz_path_dirname("file.txt");
    ASSERT_STR_EQ(".", d);
    pz_free(d);

    d = pz_path_dirname("/file.txt");
    ASSERT_STR_EQ("/", d);
    pz_free(d);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST(path_extension)
{
    pz_mem_init();

    char *e = pz_path_extension("/home/user/file.txt");
    ASSERT_STR_EQ("txt", e);
    pz_free(e);

    e = pz_path_extension("archive.tar.gz");
    ASSERT_STR_EQ("gz", e);
    pz_free(e);

    e = pz_path_extension("noextension");
    ASSERT_STR_EQ("", e);
    pz_free(e);

    e = pz_path_extension(".hidden");
    ASSERT_STR_EQ("", e);
    pz_free(e);

    e = pz_path_extension("/path/to/.hidden");
    ASSERT_STR_EQ("", e);
    pz_free(e);

    ASSERT(!pz_mem_has_leaks());
    pz_mem_shutdown();
}

TEST_MAIN()

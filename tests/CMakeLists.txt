# Tank Game - Test Suite

# Smoke tests
add_executable(test_smoke test_smoke.c)
target_include_directories(test_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME smoke COMMAND test_smoke)

# Memory system tests
add_executable(test_mem test_mem.c ../src/core/pz_mem.c)
target_include_directories(test_mem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_mem PRIVATE PZ_DEBUG)  # Always test with debug tracking
add_test(NAME mem COMMAND test_mem)

# Math library tests
add_executable(test_math test_math.c ../src/core/pz_math.c)
target_include_directories(test_math PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_math PRIVATE m)  # Link math library
add_test(NAME math COMMAND test_math)

# Data structures tests
add_executable(test_ds test_ds.c ../src/core/pz_ds.c ../src/core/pz_mem.c)
target_include_directories(test_ds PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_ds PRIVATE PZ_DEBUG)
add_test(NAME ds COMMAND test_ds)

# String utility tests
add_executable(test_str test_str.c ../src/core/pz_str.c ../src/core/pz_mem.c)
target_include_directories(test_str PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_str PRIVATE PZ_DEBUG)
add_test(NAME str COMMAND test_str)

# Platform layer tests
add_executable(test_platform test_platform.c ../src/core/pz_platform.c ../src/core/pz_mem.c)
target_include_directories(test_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_platform PRIVATE PZ_DEBUG)
add_test(NAME platform COMMAND test_platform)

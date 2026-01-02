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

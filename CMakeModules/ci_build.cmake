
set(CTEST_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/..")
set(CTEST_BINARY_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build")

set(ENV{CXXFLAGS} "--coverage -fprofile-arcs -ftest-coverage")
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_USE_LAUNCHERS 1)

set(CTEST_COVERAGE_COMMAND "gcov")
#set(CTEST_MEMORYCHECK_COMMAND "valgrind")
#set(CTEST_MEMORYCHECK_TYPE "AddressSanitizer")

ctest_start("Continuous")
ctest_configure()
ctest_build()
ctest_test(RETURN_VALUE TEST_RESULT)
if (NOT(${TEST_RESULT} EQUAL 0))
    message(SEND_ERROR "ctest_test returned: ${TEST_RESULT}")
endif (NOT(${TEST_RESULT} EQUAL 0))
#ctest_coverage()
#ctest_memcheck()
#ctest_submit()

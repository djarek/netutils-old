set (netu_tests_srcs
    netu/basic_timeout.cpp
    netu/completion_handler.cpp
    netu/synchronized_value.cpp
    netu/synchronized_stream.cpp)

function (netutils_add_test test_file)
    get_filename_component(target_name ${test_file} NAME_WE)
    add_executable(${target_name} ${test_file})
    target_link_libraries(${target_name} core Boost::unit_test_framework)
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(${target_name} PRIVATE BOOST_TEST_DYN_LINK BOOST_TEST_MODULE="${target_name}")
    target_include_directories(${target_name} PRIVATE extras/include)

    add_test(NAME "${target_name}_tests"
             COMMAND ${target_name})
endfunction(netutils_add_test)

foreach(test_src_name IN ITEMS ${netu_tests_srcs})
    netutils_add_test(${test_src_name})
endforeach()

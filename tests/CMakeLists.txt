set (netu_tests_srcs
    netu/completion_handler.cpp
)

function (netutils_add_test test_file)
    get_filename_component(target_name ${test_file} NAME_WE)
    add_executable(${target_name} ${test_file})
    target_link_libraries(${target_name} netutils ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
    target_compile_options(${target_name} PUBLIC -std=c++11 -Wall -Wextra -pedantic)
    target_compile_definitions(${target_name} PRIVATE BOOST_TEST_DYN_LINK BOOST_TEST_MODULE="${target_name}")

    target_include_directories(${target_name} PRIVATE extras/include)

    add_test("${PROJECT_NAME}_${target_name}_tests" ${target_name})
    #target_compile_features(completion_handler_test PRIVATE cxx_std_11)
endfunction(netutils_add_test)

foreach(test_src_name IN ITEMS ${netu_tests_srcs})
    netutils_add_test(${test_src_name})
endforeach()

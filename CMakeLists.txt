cmake_minimum_required(VERSION 3.6)
project(netutils LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
find_package(Boost 1.66 COMPONENTS system unit_test_framework REQUIRED)


add_library(netutils INTERFACE)
target_include_directories(netutils INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(netutils INTERFACE ${Boost_LIBRARIES} -pthread netutils)


enable_testing()
include(CTest)
add_subdirectory(tests)

install (
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp")

install(TARGETS netutils EXPORT netutilsTargets INCLUDES DESTINATION include)
install(EXPORT netutilsTargets
        FILE netutilsTargets.cmake
        NAMESPACE netu::
        DESTINATION lib/cmake/netu
)

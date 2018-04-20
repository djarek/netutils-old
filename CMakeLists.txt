cmake_minimum_required(VERSION 3.8)
project(netutils VERSION 0.1 LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)

find_package(Boost 1.67 COMPONENTS system unit_test_framework REQUIRED)
find_package (Threads)


add_library(core INTERFACE)

target_compile_features(core INTERFACE cxx_std_11)

target_include_directories(core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


target_link_libraries(core INTERFACE ${Boost_LIBRARIES} Threads::Threads)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("netutilsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES "netutilsConfig.cmake" "${CMAKE_BINARY_DIR}/netutilsConfigVersion.cmake" DESTINATION lib/cmake/netutils)

install (
    DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp")

install(TARGETS core EXPORT netutilsTargets INCLUDES DESTINATION include)
install(EXPORT netutilsTargets
        FILE netutilsTargets.cmake
        NAMESPACE netu::
        DESTINATION lib/cmake/netutils
)

enable_testing()
include(CTest)
add_subdirectory(tests)

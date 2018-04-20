include(CMakeFindDependencyMacro)

find_dependency(Boost COMPONENTS system)
include("${CMAKE_CURRENT_LIST_DIR}/netutilsTargets.cmake")

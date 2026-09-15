cmake_minimum_required(VERSION 3.13)
project(crumbs_simple_linux_controller C)

# Build this example in one of two ways:
# 1) in-tree: build inside the CRUMBS repo and link to the `crumbs` target
#    cmake -S examples/core_usage/linux/simple_controller -B build \
#          -DCRUMBS_BUILD_IN_TREE=ON
# 2) out-of-tree (installed library): set CMAKE_PREFIX_PATH to an installed
#    path that contains crumbsConfig.cmake / crumbsTargets.cmake

option(CRUMBS_BUILD_IN_TREE "Add CRUMBS as a subdirectory and link the in-repo crumbs target" ON)
if(NOT DEFINED CRUMBS_PATH)
    # Default repository path is four levels up from the example dir
    set(CRUMBS_PATH ${CMAKE_SOURCE_DIR}/../../../..)
endif()

if(CRUMBS_BUILD_IN_TREE)
    # Enable Linux HAL for I2C support
    set(CRUMBS_ENABLE_LINUX_HAL ON CACHE BOOL "" FORCE)
    # The subbuild contributes the library only; the root examples would
    # clash with this target and the tests are not wanted here.
    set(CRUMBS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CRUMBS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CRUMBS_PATH} ${CMAKE_BINARY_DIR}/crumbs_subbuild)
    
    add_executable(crumbs_simple_linux_controller simple_linux_controller.c)
    target_link_libraries(crumbs_simple_linux_controller PRIVATE crumbs)
    target_include_directories(crumbs_simple_linux_controller PRIVATE ${CRUMBS_PATH}/src)
else()
    find_package(crumbs CONFIG REQUIRED)
    add_executable(crumbs_simple_linux_controller simple_linux_controller.c)
    target_link_libraries(crumbs_simple_linux_controller PRIVATE crumbs::crumbs)
endif()

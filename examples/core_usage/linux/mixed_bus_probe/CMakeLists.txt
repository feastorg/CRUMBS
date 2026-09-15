cmake_minimum_required(VERSION 3.13)
project(crumbs_mixed_bus_probe C)

option(CRUMBS_BUILD_IN_TREE "Add CRUMBS as a subdirectory and link the in-repo crumbs target" ON)
if(NOT DEFINED CRUMBS_PATH)
    set(CRUMBS_PATH ${CMAKE_SOURCE_DIR}/../../../..)
endif()

if(CRUMBS_BUILD_IN_TREE)
    set(CRUMBS_ENABLE_LINUX_HAL ON CACHE BOOL "" FORCE)
    # The subbuild contributes the library only; the root examples would
    # clash with this target and the tests are not wanted here.
    set(CRUMBS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CRUMBS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CRUMBS_PATH} ${CMAKE_BINARY_DIR}/crumbs_subbuild)

    add_executable(crumbs_mixed_bus_probe main.c)
    target_link_libraries(crumbs_mixed_bus_probe PRIVATE crumbs)
    target_include_directories(crumbs_mixed_bus_probe PRIVATE ${CRUMBS_PATH}/src)
else()
    find_package(crumbs CONFIG REQUIRED)
    add_executable(crumbs_mixed_bus_probe main.c)
    target_link_libraries(crumbs_mixed_bus_probe PRIVATE crumbs::crumbs)
endif()


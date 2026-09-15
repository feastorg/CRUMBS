cmake_minimum_required(VERSION 3.13)
project(crumbs_controller_manual C)

# Build in-tree (inside CRUMBS repo)
option(CRUMBS_BUILD_IN_TREE "Add CRUMBS as a subdirectory" ON)
if(NOT DEFINED CRUMBS_PATH)
    # Repository path is three levels up from this example
    set(CRUMBS_PATH ${CMAKE_SOURCE_DIR}/../../..)
endif()

if(CRUMBS_BUILD_IN_TREE)
    # Enable Linux HAL for I2C support
    set(CRUMBS_ENABLE_LINUX_HAL ON CACHE BOOL "" FORCE)
    # The subbuild contributes the library only; the root examples would
    # clash with this target and the tests are not wanted here.
    set(CRUMBS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CRUMBS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CRUMBS_PATH} ${CMAKE_BINARY_DIR}/crumbs_subbuild)
    
    add_executable(crumbs_controller_manual main.c)
    target_link_libraries(crumbs_controller_manual PRIVATE crumbs)
    target_include_directories(crumbs_controller_manual PRIVATE 
        ${CRUMBS_PATH}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CRUMBS_PATH}/examples/families_usage
    )
else()
    find_package(crumbs CONFIG REQUIRED)
    add_executable(crumbs_controller_manual main.c)
    target_link_libraries(crumbs_controller_manual PRIVATE crumbs::crumbs)
    target_include_directories(crumbs_controller_manual PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CRUMBS_PATH}/examples/families_usage
    )
endif()

install(TARGETS crumbs_controller_manual DESTINATION bin)

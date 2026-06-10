cmake_minimum_required(VERSION 3.13)

project(crumbs_core_only_consumer C)

find_package(crumbs CONFIG REQUIRED)

add_executable(core_only_consumer main.c)
target_link_libraries(core_only_consumer PRIVATE crumbs::crumbs)

cmake_minimum_required(VERSION 3.13)

project(CRUMBS
    VERSION 0.12.2
    LANGUAGES C
)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

option(CRUMBS_ENABLE_LINUX_HAL "Enable Linux I2C HAL (requires linux-wire)" OFF)
option(CRUMBS_BUILD_EXAMPLES   "Build Linux example programs"               ON)
option(CRUMBS_ENABLE_TESTS     "Build and run the lightweight C tests"       ON)

# -----------------------------------------------------------------------------
# Global C settings
# -----------------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------

set(CRUMBS_CORE_SOURCES
    src/core/crumbs_core.c
    src/core/crumbs_i2c_helpers.c
    src/crc/crumbs_crc.c
    src/crc/crc8_nibble.c
)

if(CRUMBS_ENABLE_LINUX_HAL)
    list(APPEND CRUMBS_CORE_SOURCES
        src/hal/linux/crumbs_i2c_linux.c
    )
endif()

# -----------------------------------------------------------------------------
# Library target
# -----------------------------------------------------------------------------

add_library(crumbs STATIC ${CRUMBS_CORE_SOURCES})
add_library(crumbs::crumbs ALIAS crumbs)

target_include_directories(crumbs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include/crumbs>
)

# -----------------------------------------------------------------------------
# Linux HAL dependency
# -----------------------------------------------------------------------------

if(CRUMBS_ENABLE_LINUX_HAL)
    if(NOT TARGET linux_wire::linux_wire)
        if(TARGET linux_wire)
            add_library(linux_wire::linux_wire INTERFACE IMPORTED)
            set_target_properties(linux_wire::linux_wire PROPERTIES
                INTERFACE_LINK_LIBRARIES linux_wire
            )
        else()
            find_package(linux_wire CONFIG REQUIRED)
        endif()
    endif()

    target_link_libraries(crumbs PUBLIC linux_wire::linux_wire)
endif()

# -----------------------------------------------------------------------------
# Examples (Linux only)
# -----------------------------------------------------------------------------

if(CRUMBS_BUILD_EXAMPLES AND CRUMBS_ENABLE_LINUX_HAL)
    add_executable(crumbs_simple_linux_controller
        examples/core_usage/linux/simple_controller/simple_linux_controller.c
    )
    target_link_libraries(crumbs_simple_linux_controller PRIVATE crumbs)

    add_executable(crumbs_mixed_bus_probe
        examples/core_usage/linux/mixed_bus_probe/main.c
    )
    target_link_libraries(crumbs_mixed_bus_probe PRIVATE crumbs)

    add_executable(crumbs_mixed_bus_lab_validation
        examples/core_usage/linux/mixed_bus_lab_validation/main.c
    )
    target_link_libraries(crumbs_mixed_bus_lab_validation PRIVATE crumbs)

    add_executable(crumbs_mock_controller
        examples/handlers_usage/linux/mock_controller/mock_controller.c
    )
    target_link_libraries(crumbs_mock_controller PRIVATE crumbs)
    target_include_directories(crumbs_mock_controller PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/handlers_usage
    )

    add_executable(crumbs_controller_discovery
        examples/families_usage/controller_discovery/main.c
    )
    target_link_libraries(crumbs_controller_discovery PRIVATE crumbs)
    target_include_directories(crumbs_controller_discovery PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/families_usage
    )

    add_executable(crumbs_controller_manual
        examples/families_usage/controller_manual/main.c
    )
    target_link_libraries(crumbs_controller_manual PRIVATE crumbs)
    target_include_directories(crumbs_controller_manual PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/families_usage/controller_manual
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/families_usage
    )
endif()

# -----------------------------------------------------------------------------
# Tests (hardware-independent)
# -----------------------------------------------------------------------------

if(CRUMBS_ENABLE_TESTS)
    enable_testing()

    add_executable(test_encode_decode tests/test_encode_decode.c)
    target_link_libraries(test_encode_decode PRIVATE crumbs)
    add_test(NAME encode_decode_test COMMAND test_encode_decode)

    add_executable(test_scan_fake tests/test_scan_fake.c)
    target_link_libraries(test_scan_fake PRIVATE crumbs)
    add_test(NAME scan_fake_test COMMAND test_scan_fake)

    add_executable(test_i2c_helpers tests/test_i2c_helpers.c)
    target_link_libraries(test_i2c_helpers PRIVATE crumbs)
    add_test(NAME i2c_helpers_test COMMAND test_i2c_helpers)

    add_executable(test_crc_failure tests/test_crc_failure.c)
    target_link_libraries(test_crc_failure PRIVATE crumbs)
    add_test(NAME crc_failure_test COMMAND test_crc_failure)

    add_executable(test_handlers tests/test_handlers.c)
    target_link_libraries(test_handlers PRIVATE crumbs)
    add_test(NAME handlers_test COMMAND test_handlers)

    add_executable(test_msg_helpers tests/test_msg_helpers.c)
    target_link_libraries(test_msg_helpers PRIVATE crumbs)
    # Link math library on non-MSVC platforms (MSVC has it built-in)
    if(NOT MSVC)
        target_link_libraries(test_msg_helpers PRIVATE m)
    endif()
    add_test(NAME msg_helpers_test COMMAND test_msg_helpers)

    add_executable(test_context tests/test_context.c)
    target_link_libraries(test_context PRIVATE crumbs)
    add_test(NAME context_test COMMAND test_context)

    add_executable(test_peripheral_flow tests/test_peripheral_flow.c)
    target_link_libraries(test_peripheral_flow PRIVATE crumbs)
    add_test(NAME peripheral_flow_test COMMAND test_peripheral_flow)

    add_executable(test_version tests/test_version.c)
    target_link_libraries(test_version PRIVATE crumbs)
    add_test(NAME version_test COMMAND test_version)

    add_executable(test_set_reply tests/test_set_reply.c)
    target_link_libraries(test_set_reply PRIVATE crumbs)
    add_test(NAME set_reply_test COMMAND test_set_reply)

    add_executable(test_reply_flow tests/test_reply_flow.c)
    target_link_libraries(test_reply_flow PRIVATE crumbs)
    add_test(NAME reply_flow_test COMMAND test_reply_flow)

    add_executable(test_reply_handler tests/test_reply_handler.c)
    target_link_libraries(test_reply_handler PRIVATE crumbs)
    add_test(NAME reply_handler_test COMMAND test_reply_handler)
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------

install(TARGETS crumbs
    EXPORT crumbsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION include/crumbs
)

install(FILES
    src/crumbs.h
    src/crumbs_arduino.h
    src/crumbs_crc.h
    src/crumbs_i2c.h
    src/crumbs_linux.h
    src/crumbs_message.h
    src/crumbs_message_helpers.h
    src/crumbs_ops.h
    src/crumbs_version.h
    DESTINATION include/crumbs
)

# -----------------------------------------------------------------------------
# Package Config Export
# -----------------------------------------------------------------------------

set(config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/crumbs")

if(CRUMBS_ENABLE_LINUX_HAL)
    set(CRUMBS_PACKAGE_DEPENDENCIES "find_dependency(linux_wire CONFIG REQUIRED)")
else()
    set(CRUMBS_PACKAGE_DEPENDENCIES "# no extra dependencies")
endif()

configure_package_config_file(
    cmake/crumbsConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/crumbsConfig.cmake"
    INSTALL_DESTINATION ${config_install_dir}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/crumbsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT crumbsTargets
    FILE crumbsTargets.cmake
    NAMESPACE crumbs::
    DESTINATION ${config_install_dir}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/crumbsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/crumbsConfigVersion.cmake"
    DESTINATION ${config_install_dir}
)


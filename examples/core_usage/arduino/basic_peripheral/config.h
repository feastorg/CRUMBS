#pragma once

#include <stddef.h>
#include <stdint.h>

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t HEARTBEAT_INTERVAL_MS = 500;

static const uint8_t DEVICE_ADDR = 0x10;
static const uint8_t DEVICE_TYPE_ID = 0x01;

static const uint8_t CMD_STORE_DATA = 0x01;
static const uint8_t CMD_CLEAR_DATA = 0x02;

static const uint8_t QUERY_VERSION_OPCODE = 0x00;
static const uint8_t QUERY_STORED_DATA_OPCODE = 0x80;

static const uint8_t MODULE_VERSION_MAJOR = 1;
static const uint8_t MODULE_VERSION_MINOR = 0;
static const uint8_t MODULE_VERSION_PATCH = 0;

static const size_t STORED_DATA_CAPACITY = 10u;

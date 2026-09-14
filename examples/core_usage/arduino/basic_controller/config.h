#pragma once

#include <stddef.h>
#include <stdint.h>

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t HEARTBEAT_INTERVAL_MS = 500;

static const uint8_t TARGET_ADDR = 0x10;
static const uint8_t TARGET_TYPE_ID = 0x01;

static const uint8_t CMD_STORE_DATA = 0x01;
static const uint8_t CMD_CLEAR_DATA = 0x02;
static const uint8_t QUERY_VERSION_OPCODE = 0x00;
static const uint8_t QUERY_STORED_DATA_OPCODE = 0x80;

static const uint8_t STORE_PAYLOAD[] = {0xAA, 0xBB, 0xCC};

static const uint32_t QUERY_DELAY_MS = 10;
static const size_t READ_BUFFER_LEN = 32u;
static const uint32_t READ_TIMEOUT_US = 5000;

static const char CMD_KEY_STORE = 's';
static const char CMD_KEY_CLEAR = 'c';
static const char CMD_KEY_QUERY_VERSION = 'v';
static const char CMD_KEY_QUERY_STORED = 'd';

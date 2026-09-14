#pragma once

#include <stddef.h>
#include <stdint.h>

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t HEARTBEAT_INTERVAL_MS = 500;
static const uint32_t STATUS_INTERVAL_MS = 5000;

static const uint32_t CRUMBS_QUERY_DELAY_MS = 10;
static const uint32_t CRUMBS_READ_TIMEOUT_US = 10000;
// Version/query replies are small; keep this tight for AVR stack usage.
static const size_t CRUMBS_READ_BUFFER_LEN = 16u;

static const uint8_t CRUMBS_QUERY_OPCODE = 0x00;
static const uint8_t kCrumbsCandidates[] = {0x10, 0x11, 0x12};
// Keep found-buffer bounded to candidate profile for low-RAM AVR targets.
static const size_t MAX_CRUMBS_FOUND = 3u;

// Atlas Scientific EZO default I2C addresses (devices are in I2C mode).
static const uint8_t EZO_PH_ADDR = 0x63; // decimal 99
static const uint8_t EZO_DO_ADDR = 0x61; // decimal 97

// Match ezo-driver Arduino examples: allow device startup settle before first command/read.
static const uint32_t EZO_STARTUP_SETTLE_MS = 1000;

// Bosch BMP/BME280 I2C addresses.
static const uint8_t kBoschSensorAddrs[] = {0x76, 0x77};
static const size_t MAX_BOSCH_SENSORS = sizeof(kBoschSensorAddrs) / sizeof(kBoschSensorAddrs[0]);
// Default keeps both Bosch sensor slots active for full functionality.
static const size_t BOSCH_SENSOR_COUNT = MAX_BOSCH_SENSORS;

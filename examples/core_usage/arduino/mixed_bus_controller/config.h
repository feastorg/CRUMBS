#pragma once

#include <stddef.h>
#include <stdint.h>

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t HEARTBEAT_INTERVAL_MS = 500;
static const uint32_t SENSOR_READ_TIMEOUT_US = 10000;
static const uint32_t STATUS_INTERVAL_MS = 5000;
static const uint32_t CRUMBS_QUERY_DELAY_MS = 10;
static const uint32_t CRUMBS_READ_TIMEOUT_US = 10000;
static const size_t CRUMBS_READ_BUFFER_LEN = 32u;

// Bosch BMP280/BME280 register-based reference sensor.
// Address selection from datasheet:
// - SDO=GND   -> 0x76
// - SDO=VDDIO -> 0x77
static const uint8_t kSensorAddrs[] = {0x76, 0x77};
static const size_t SENSOR_COUNT = 2u;

// Chip-ID register and expected production values from Bosch datasheets.
static const uint8_t SENSOR_CHIP_ID_REG = 0xD0;
static const uint8_t SENSOR_CTRL_HUM_REG = 0xF2;
static const uint8_t SENSOR_CTRL_MEAS_REG = 0xF4;
static const uint8_t SENSOR_RAW_DATA_REG = 0xF7;
static const uint8_t BMP280_CHIP_ID = 0x58;
static const uint8_t BME280_CHIP_ID = 0x60;
static const uint8_t SENSOR_CTRL_HUM_X1 = 0x01;
static const uint8_t SENSOR_CTRL_MEAS_NORMAL_X1 = 0x27;
static const size_t BMP280_RAW_LEN = 6u;
static const size_t BME280_RAW_LEN = 8u;

// Probe only known CRUMBS addresses on mixed buses.
static const uint8_t kCrumbsCandidates[] = {0x10, 0x11, 0x12};
static const uint8_t CRUMBS_QUERY_OPCODE = 0x00;

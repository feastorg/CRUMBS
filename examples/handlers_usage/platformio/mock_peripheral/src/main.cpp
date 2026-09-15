/**
 * @file main.cpp
 * @brief Mock device peripheral using handler pattern.
 *
 * This example demonstrates:
 * - Using crumbs_register_handler() for SET operations (0x01-0x7F)
 * - Using on_request callback for GET operations (via SET_REPLY + I2C read)
 * - Handler-based command dispatch
 * - State management and data persistence
 *
 * Hardware:
 * - Arduino Nano (or compatible)
 * - I2C address: 0x10
 * - Built-in LED pulses at configurable heartbeat rate when enabled
 *
 * Operations:
 * - MOCK_OP_ECHO (0x01): Stores data for later retrieval
 * - MOCK_OP_SET_HEARTBEAT (0x02): Sets LED heartbeat period
 * - MOCK_OP_TOGGLE (0x03): Enables/disables LED heartbeat
 * - MOCK_OP_GET_ECHO (0x80): Returns stored echo data
 * - MOCK_OP_GET_STATUS (0x81): Returns heartbeat state and period
 * - MOCK_OP_GET_INFO (0x82): Returns device info string
 *
 * Memory optimization:
 * - CRUMBS_MAX_HANDLERS=8 reduces RAM usage
 * - Set in platformio.ini build_flags
 */

#include <Arduino.h>
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

/* LED_BUILTIN is not defined on all boards (e.g. bare ESP32 dev modules) */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

/* Include contract header (from parent directory) */
#include "mock_ops.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define PERIPHERAL_ADDR 0x10

/* ============================================================================
 * State
 * ============================================================================ */

static crumbs_context_t ctx;
static uint8_t g_state = 0;           /* Heartbeat enable (0=off, 1=on) */
static uint16_t g_heartbeat_ms = 500; /* Heartbeat period in milliseconds */
static uint8_t g_echo_buf[27];        /* Echo data buffer */
static uint8_t g_echo_len = 0;        /* Echo data length */

/* ============================================================================
 * Handler: Echo (MOCK_OP_ECHO)
 * ============================================================================ */

/**
 * @brief Handler for MOCK_OP_ECHO - stores data for later retrieval.
 */
static void handler_echo(crumbs_context_t *ctx, uint8_t opcode,
                         const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    /* Store echo data */
    g_echo_len = data_len;
    if (g_echo_len > sizeof(g_echo_buf))
    {
        g_echo_len = sizeof(g_echo_buf);
    }

    memcpy(g_echo_buf, data, g_echo_len);
}

/* ============================================================================
 * Handler: Set Heartbeat (MOCK_OP_SET_HEARTBEAT)
 * ============================================================================ */

/**
 * @brief Handler for MOCK_OP_SET_HEARTBEAT - sets LED heartbeat period.
 */
static void handler_set_heartbeat(crumbs_context_t *ctx, uint8_t opcode,
                                  const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    uint16_t period;
    if (crumbs_msg_read_u16(data, data_len, 0, &period) == 0)
    {
        g_heartbeat_ms = period;
    }
}

/* ============================================================================
 * Handler: Toggle (MOCK_OP_TOGGLE)
 * ============================================================================ */

/**
 * @brief Handler for MOCK_OP_TOGGLE - enables/disables LED heartbeat.
 */
static void handler_toggle(crumbs_context_t *ctx, uint8_t opcode,
                           const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)data;
    (void)data_len;
    (void)user_data;

    g_state ^= 1;
}

/* ============================================================================
 * Request Handler (GET operations via SET_REPLY)
 * ============================================================================ */

/**
 * @brief Request handler for I2C read operations.
 *
 * Switches on ctx->requested_opcode (set by controller via SET_REPLY).
 */
static void on_request(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    switch (ctx->requested_opcode)
    {
    case 0: /* Default reply - device info */
    case MOCK_OP_GET_INFO:
    {
        const char *info = "MockDev v1.0";
        crumbs_msg_init(reply, MOCK_TYPE_ID, MOCK_OP_GET_INFO);
        for (const char *p = info; *p; p++)
        {
            crumbs_msg_add_u8(reply, (uint8_t)*p);
        }
        break;
    }

    case MOCK_OP_GET_ECHO:
    {
        crumbs_msg_init(reply, MOCK_TYPE_ID, MOCK_OP_GET_ECHO);
        for (uint8_t i = 0; i < g_echo_len; i++)
        {
            crumbs_msg_add_u8(reply, g_echo_buf[i]);
        }
        break;
    }

    case MOCK_OP_GET_STATUS:
    {
        crumbs_msg_init(reply, MOCK_TYPE_ID, MOCK_OP_GET_STATUS);
        crumbs_msg_add_u8(reply, g_state);
        crumbs_msg_add_u16(reply, g_heartbeat_ms);
        break;
    }

    default:
        /* Unknown opcode - return empty message */
        crumbs_msg_init(reply, MOCK_TYPE_ID, ctx->requested_opcode);
        break;
    }
}

/* ============================================================================
 * Setup & Loop
 * ============================================================================ */

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(1);
    }

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.print("CRUMBS Mock Peripheral - address 0x");
    if (PERIPHERAL_ADDR < 0x10)
        Serial.print('0');
    Serial.println(PERIPHERAL_ADDR, HEX);

    /* Initialize CRUMBS context */
    crumbs_arduino_init_peripheral(&ctx, PERIPHERAL_ADDR);
    crumbs_set_type_id(&ctx, MOCK_TYPE_ID); /* drop frames addressed to other families */

    /* Register SET operation handlers */
    crumbs_register_handler(&ctx, MOCK_OP_ECHO, handler_echo, nullptr);
    crumbs_register_handler(&ctx, MOCK_OP_SET_HEARTBEAT, handler_set_heartbeat, nullptr);
    crumbs_register_handler(&ctx, MOCK_OP_TOGGLE, handler_toggle, nullptr);

    /* Register GET operation callback */
    crumbs_set_callbacks(&ctx, nullptr, on_request, nullptr);

    Serial.println("Ready");
}

void loop()
{
    /* Peripheral work is interrupt-driven by Wire callbacks */

    /* Implement LED heartbeat if enabled */
    if (g_state && g_heartbeat_ms > 0)
    {
        static unsigned long last_pulse = 0;
        unsigned long now = millis();

        if (now - last_pulse >= g_heartbeat_ms)
        {
            last_pulse = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
        else if (now - last_pulse >= 50) /* 50ms pulse width */
        {
            digitalWrite(LED_BUILTIN, LOW);
        }
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
    }

    delay(10);
}

/**
 * @file main.cpp
 * @brief Quad 7-segment display peripheral using CRUMBS handler pattern.
 *
 * This peripheral demonstrates a display control interface with:
 * - SET operations (0x01-0x04): Display numbers, segments, brightness, clear
 * - GET operations (0x80): Query current displayed value via SET_REPLY
 * - 4-digit 7-segment display (5641AS or compatible)
 * - Continuous display refresh via multiplexing
 *
 * Hardware:
 * - Arduino Nano (ATmega328P)
 * - I2C address: 0x40
 * - 5641AS quad 7-segment display
 * - Segment pins: D9,D13,D4,D6,D7,D10,D3,D5 (a,b,c,d,e,f,g,dp), see segmentPins[]
 * - Digit select pins: D8,D11,D12,D2 (digits 1-4), see digitSelectionPins[]
 * - I2C pins: A4 (SDA), A5 (SCL)
 *
 * Operations:
 * - DISPLAY_OP_SET_NUMBER (0x01): Display number with decimal point
 * - DISPLAY_OP_SET_SEGMENTS (0x02): Set custom segment patterns
 * - DISPLAY_OP_SET_BRIGHTNESS (0x03): Set brightness level
 * - DISPLAY_OP_CLEAR (0x04): Clear display
 * - DISPLAY_OP_GET_VALUE (0x80): Query current value
 */

#include <Arduino.h>
#include <Wire.h>
#include <Simple5641AS.h>
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

/* Include contract header (from parent directory) */
#include "display_ops.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define PERIPHERAL_ADDR 0x40

/* Pin configuration for 5641AS display */
// segmentPins[] = { a, b, c, d, e, f, g, dot };
const uint8_t segmentPins[] = {9, 13, 4, 6, 7, 10, 3, 5};
// digitSelectionPins[] = { D1, D2, D3, D4 };
const uint8_t digitSelectionPins[] = {8, 11, 12, 2};

/* ============================================================================
 * State
 * ============================================================================ */

static crumbs_context_t ctx;
static Simple5641AS display(segmentPins, digitSelectionPins);

static uint16_t g_current_number = 0;      /* Current displayed number */
static uint8_t g_decimal_pos = 0;          /* Decimal position (0=none, 1-4) */
static uint8_t g_brightness = 5;           /* Brightness level (0-10) */
static uint8_t g_custom_segments[4] = {0}; /* Custom segment patterns */
static bool g_use_custom_segments = false; /* Use custom patterns vs number */
static bool g_display_active = false;      /* Display is showing something */

/* Display refresh timing */
static unsigned long g_last_refresh = 0;
static const unsigned long REFRESH_INTERVAL_US = 2000; /* 2ms per digit cycle */

/* ============================================================================
 * Display Control
 * ============================================================================ */

/**
 * @brief Refresh the display (call frequently from loop).
 *
 * This handles multiplexing of the 4 digits. Must be called regularly
 * to prevent flickering.
 */
static void refresh_display(void)
{
    unsigned long now = micros();

    if (now - g_last_refresh < REFRESH_INTERVAL_US)
    {
        return; /* Not time to refresh yet */
    }

    g_last_refresh = now;

    if (!g_display_active)
    {
        display.clean(); /* Clear display if inactive */
        return;
    }

    if (g_use_custom_segments)
    {
        /* Display custom segment patterns */
        display.customDisplayCycle(500, g_custom_segments);
    }
    else
    {
        /* Display number with optional decimal point */
        /* Library uses dot=0 for leftmost digit, our API uses 1-4 for digit 1-4 */
        int dot_digit = (g_decimal_pos >= 1 && g_decimal_pos <= 4) ? (g_decimal_pos - 1) : -1;
        display.cycle(500, g_current_number, dot_digit);
    }
}

/**
 * @brief Set the displayed number.
 */
static void set_display_number(uint16_t number, uint8_t decimal_pos)
{
    g_current_number = number;
    g_decimal_pos = decimal_pos;
    g_use_custom_segments = false;
    g_display_active = true;

    Serial.print(F("Display: "));
    Serial.print(number);
    if (decimal_pos > 0 && decimal_pos <= 4)
    {
        Serial.print(F(" (decimal on digit "));
        Serial.print(decimal_pos);
        Serial.print(F(")"));
    }
    Serial.println();
}

/**
 * @brief Set custom segment patterns.
 */
static void set_custom_segments(const uint8_t segments[4])
{
    for (int i = 0; i < 4; i++)
    {
        g_custom_segments[i] = segments[i];
    }
    g_use_custom_segments = true;
    g_display_active = true;

    Serial.print(F("Custom segments: 0x"));
    for (int i = 0; i < 4; i++)
    {
        Serial.print(g_custom_segments[i], HEX);
        Serial.print(F(" "));
    }
    Serial.println();
}

/**
 * @brief Set brightness level.
 */
static void set_brightness(uint8_t level)
{
    if (level > 10)
        level = 10;

    g_brightness = level;

    /* Note: Simple5641AS library doesn't have brightness control,
     * so this is stored but not actively used. For hardware PWM,
     * you'd control digit select pins with PWM here. */

    Serial.print(F("Brightness: "));
    Serial.println(level);
}

/**
 * @brief Clear the display.
 */
static void clear_display(void)
{
    g_display_active = false;
    display.clean();

    Serial.println(F("Display cleared"));
}

/* ============================================================================
 * Handler: SET_NUMBER (DISPLAY_OP_SET_NUMBER)
 * ============================================================================ */

static void handler_set_number(crumbs_context_t *ctx, uint8_t opcode,
                               const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    display_set_number_t v;
    if (display_set_number_unpack(data, data_len, &v) != 0)
    {
        Serial.println(F("SET_NUMBER: Invalid payload"));
        return;
    }

    set_display_number(v.number, v.decimal_pos);
}

/* ============================================================================
 * Handler: SET_SEGMENTS (DISPLAY_OP_SET_SEGMENTS)
 * ============================================================================ */

static void handler_set_segments(crumbs_context_t *ctx, uint8_t opcode,
                                 const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    display_set_segments_t v;
    if (display_set_segments_unpack(data, data_len, &v) != 0)
    {
        Serial.println(F("SET_SEGMENTS: Invalid payload"));
        return;
    }

    const uint8_t segments[4] = {v.digit0, v.digit1, v.digit2, v.digit3};
    set_custom_segments(segments);
}

/* ============================================================================
 * Handler: SET_BRIGHTNESS (DISPLAY_OP_SET_BRIGHTNESS)
 * ============================================================================ */

static void handler_set_brightness(crumbs_context_t *ctx, uint8_t opcode,
                                   const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    display_set_brightness_t v;
    if (display_set_brightness_unpack(data, data_len, &v) != 0)
    {
        Serial.println(F("SET_BRIGHTNESS: Invalid payload"));
        return;
    }

    set_brightness(v.level);
}

/* ============================================================================
 * Handler: CLEAR (DISPLAY_OP_CLEAR)
 * ============================================================================ */

static void handler_clear(crumbs_context_t *ctx, uint8_t opcode,
                          const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)data;
    (void)data_len;
    (void)user_data;

    clear_display();
}

/* ============================================================================
 * Reply Handlers (GET operations via SET_REPLY)
 * ============================================================================ */

static void reply_handler_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_build_version_reply(reply, DISPLAY_TYPE_ID,
                               DISPLAY_MODULE_VER_MAJOR,
                               DISPLAY_MODULE_VER_MINOR,
                               DISPLAY_MODULE_VER_PATCH);
    Serial.print(F("Version: CRUMBS="));
    Serial.print(CRUMBS_VERSION);
    Serial.print(F(" Module="));
    Serial.print(DISPLAY_MODULE_VER_MAJOR);
    Serial.print(F("."));
    Serial.print(DISPLAY_MODULE_VER_MINOR);
    Serial.print(F("."));
    Serial.println(DISPLAY_MODULE_VER_PATCH);
}

static void reply_handler_get_value(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    display_value_result_t r;
    r.number = g_current_number;
    r.decimal_pos = g_decimal_pos;
    r.brightness = g_brightness;
    crumbs_msg_init(reply, DISPLAY_TYPE_ID, DISPLAY_OP_GET_VALUE);
    display_value_result_pack(reply, &r);
    Serial.print(F("GET_VALUE: number="));
    Serial.print(g_current_number);
    Serial.print(F(" decimal="));
    Serial.print(g_decimal_pos);
    Serial.print(F(" brightness="));
    Serial.println(g_brightness);
}

/* ============================================================================
 * Setup
 * ============================================================================ */

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ; /* Wait for serial or timeout */

    Serial.println(F("\n=== LHWIT Display Peripheral ==="));
    Serial.print(F("Type ID: 0x"));
    Serial.println(DISPLAY_TYPE_ID, HEX);
    Serial.print(F("Address: 0x"));
    Serial.println(PERIPHERAL_ADDR, HEX);
    Serial.print(F("Version: "));
    Serial.print(DISPLAY_MODULE_VER_MAJOR);
    Serial.print(F("."));
    Serial.print(DISPLAY_MODULE_VER_MINOR);
    Serial.print(F("."));
    Serial.println(DISPLAY_MODULE_VER_PATCH);
    Serial.print(F("CRUMBS: "));
    Serial.println(CRUMBS_VERSION_STRING);
    Serial.println(F("==============================\n"));

    /* Initialize display */
    display.clean();
    g_display_active = false;

    /* Initialize CRUMBS peripheral */
    crumbs_arduino_init_peripheral(&ctx, PERIPHERAL_ADDR);
    crumbs_set_type_id(&ctx, DISPLAY_TYPE_ID); /* drop frames addressed to other families */

    /* Register SET operation handlers */
    crumbs_register_handler(&ctx, DISPLAY_OP_SET_NUMBER, handler_set_number, NULL);
    crumbs_register_handler(&ctx, DISPLAY_OP_SET_SEGMENTS, handler_set_segments, NULL);
    crumbs_register_handler(&ctx, DISPLAY_OP_SET_BRIGHTNESS, handler_set_brightness, NULL);
    crumbs_register_handler(&ctx, DISPLAY_OP_CLEAR, handler_clear, NULL);

    /* Register GET operation reply handlers */
    crumbs_register_reply_handler(&ctx, 0x00,                reply_handler_version,   NULL);
    crumbs_register_reply_handler(&ctx, DISPLAY_OP_GET_VALUE, reply_handler_get_value, NULL);

    Serial.println(F("Display peripheral ready"));
    Serial.println(F("Waiting for commands...\n"));

    /* Show a brief boot sequence */
    set_display_number(8888, 0);
    delay(500);
    clear_display();
}

/* ============================================================================
 * Loop
 * ============================================================================ */

void loop()
{
    /* Refresh display continuously (multiplexing) */
    refresh_display();

    /* Small delay to prevent tight loop */
    delayMicroseconds(100);
}

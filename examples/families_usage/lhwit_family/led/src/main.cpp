/**
 * @file main.cpp
 * @brief LED array peripheral using CRUMBS handler pattern.
 *
 * This peripheral demonstrates a state-query interface with:
 * - SET operations (0x01-0x03): Control LED states and blink patterns
 * - GET operations (0x80-0x81): Query current states via SET_REPLY
 * - 4 LEDs on GPIO pins (configurable)
 * - Blink pattern support
 *
 * Hardware:
 * - Arduino Nano (ATmega328P)
 * - I2C address: 0x20
 * - 4 LEDs on D4-D7 (with current-limiting resistors)
 * - I2C pins: A4 (SDA), A5 (SCL)
 *
 * Operations:
 * - LED_OP_SET_ALL (0x01): Set all LEDs using bitmask
 * - LED_OP_SET_ONE (0x02): Set individual LED state
 * - LED_OP_BLINK (0x03): Configure LED blinking
 * - LED_OP_GET_STATE (0x80): Query current LED states
 * - LED_OP_GET_BLINK (0x81): Query blink configuration
 */

#include <Arduino.h>
#include <Wire.h>
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

/* Include contract header (from parent directory) */
#include "led_ops.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define PERIPHERAL_ADDR 0x20
#define NUM_LEDS 4

/* GPIO pins for LEDs (D4-D7 on Arduino Nano) */
static const uint8_t LED_PINS[NUM_LEDS] = {4, 5, 6, 7};

/* ============================================================================
 * Blink State Structure
 * ============================================================================ */

typedef struct
{
    uint8_t enable;        /* 0=disabled, 1=enabled */
    uint16_t period_ms;    /* Blink period in milliseconds */
    uint32_t last_toggle;  /* Last toggle time (millis()) */
    uint8_t current_state; /* Current blink state (0=off, 1=on) */
} led_blink_state_t;

/* ============================================================================
 * State
 * ============================================================================ */

static crumbs_context_t ctx;
static uint8_t g_led_states = 0;            /* Current LED states (bit mask) */
static led_blink_state_t g_blink[NUM_LEDS]; /* Blink state per LED */

/* ============================================================================
 * Hardware Control
 * ============================================================================ */

/**
 * @brief Set physical LED state.
 *
 * @param led_idx LED index (0-3)
 * @param state   LED state (0=OFF, 1=ON)
 */
static void set_led_physical(uint8_t led_idx, uint8_t state)
{
    if (led_idx >= NUM_LEDS)
    {
        return;
    }

    digitalWrite(LED_PINS[led_idx], state ? HIGH : LOW);
}

/**
 * @brief Update all LEDs based on current state.
 */
static void update_all_leds(void)
{
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t state = (g_led_states >> i) & 1;
        set_led_physical(i, state);
    }
}

/* ============================================================================
 * Handler: SET_ALL (LED_OP_SET_ALL)
 * ============================================================================ */

/**
 * @brief Handler for SET_ALL - sets all LEDs using bitmask.
 */
static void handler_set_all(crumbs_context_t *ctx, uint8_t opcode,
                            const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    if (data_len < 1)
    {
        return;
    }

    uint8_t mask = data[0];

    /* Update state */
    g_led_states = mask & 0x0F; /* Only lower 4 bits */

    /* Update hardware */
    update_all_leds();
}

/* ============================================================================
 * Handler: SET_ONE (LED_OP_SET_ONE)
 * ============================================================================ */

/**
 * @brief Handler for SET_ONE - sets individual LED state.
 */
static void handler_set_one(crumbs_context_t *ctx, uint8_t opcode,
                            const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    if (data_len < 2)
    {
        return;
    }

    uint8_t led_idx = data[0];
    uint8_t state = data[1];

    if (led_idx >= NUM_LEDS)
    {
        return;
    }

    /* Update state */
    if (state)
    {
        g_led_states |= (1 << led_idx); /* Set bit */
    }
    else
    {
        g_led_states &= ~(1 << led_idx); /* Clear bit */
    }

    /* Update hardware */
    set_led_physical(led_idx, state);
}

/* ============================================================================
 * Handler: BLINK (LED_OP_BLINK)
 * ============================================================================ */

/**
 * @brief Handler for BLINK - configures LED blinking.
 */
static void handler_blink(crumbs_context_t *ctx, uint8_t opcode,
                          const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    if (data_len < 4)
    {
        return;
    }

    uint8_t led_idx = data[0];
    uint8_t enable = data[1];
    uint16_t period_ms;

    if (crumbs_msg_read_u16(data, data_len, 2, &period_ms) != 0)
    {
        return;
    }

    if (led_idx >= NUM_LEDS)
    {
        return;
    }

    /* Update blink configuration */
    g_blink[led_idx].enable = enable ? 1 : 0;
    g_blink[led_idx].period_ms = period_ms;
    g_blink[led_idx].last_toggle = millis();
    g_blink[led_idx].current_state = 0;
}

/* ============================================================================
 * Reply Handlers (GET operations via SET_REPLY)
 * ============================================================================ */

static void reply_handler_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_build_version_reply(reply, LED_TYPE_ID,
                               LED_MODULE_VER_MAJOR,
                               LED_MODULE_VER_MINOR,
                               LED_MODULE_VER_PATCH);
}

static void reply_handler_get_state(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, LED_TYPE_ID, LED_OP_GET_STATE);
    crumbs_msg_add_u8(reply, g_led_states);
}

static void reply_handler_get_blink(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, LED_TYPE_ID, LED_OP_GET_BLINK);
    /* Pack blink configuration: [enable:u8][period:u16] x 4 LEDs = 12 bytes */
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        crumbs_msg_add_u8(reply, g_blink[i].enable);
        crumbs_msg_add_u16(reply, g_blink[i].period_ms);
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
        delay(10);
    }

    Serial.println("\n=== CRUMBS LED Array Peripheral ===");
    Serial.print("I2C Address: 0x");
    Serial.println(PERIPHERAL_ADDR, HEX);
    Serial.println();

    /* Initialize LED GPIO pins */
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);

        /* Initialize blink state */
        g_blink[i].enable = 0;
        g_blink[i].period_ms = 1000;
        g_blink[i].last_toggle = 0;
        g_blink[i].current_state = 0;
    }

    /* Initialize CRUMBS peripheral */
    crumbs_arduino_init_peripheral(&ctx, PERIPHERAL_ADDR);
    crumbs_set_type_id(&ctx, LED_TYPE_ID); /* drop frames addressed to other families */

    /* Register SET operation handlers */
    crumbs_register_handler(&ctx, LED_OP_SET_ALL, handler_set_all, nullptr);
    crumbs_register_handler(&ctx, LED_OP_SET_ONE, handler_set_one, nullptr);
    crumbs_register_handler(&ctx, LED_OP_BLINK, handler_blink, nullptr);

    /* Register GET operation reply handlers */
    crumbs_register_reply_handler(&ctx, 0,                reply_handler_version,   nullptr);
    crumbs_register_reply_handler(&ctx, LED_OP_GET_STATE, reply_handler_get_state, nullptr);
    crumbs_register_reply_handler(&ctx, LED_OP_GET_BLINK, reply_handler_get_blink, nullptr);

    Serial.println("Ready");
}

void loop()
{
    /* Handle blink timing for each LED */
    uint32_t now = millis();

    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        if (g_blink[i].enable && g_blink[i].period_ms > 0)
        {
            uint32_t half_period = g_blink[i].period_ms / 2;

            if (now - g_blink[i].last_toggle >= half_period)
            {
                g_blink[i].last_toggle = now;
                g_blink[i].current_state = !g_blink[i].current_state;

                /* Toggle physical LED (overrides static state during blink) */
                set_led_physical(i, g_blink[i].current_state);
            }
        }
    }

    delay(10);
}

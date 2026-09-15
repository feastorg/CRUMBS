/**
 * @file main.cpp
 * @brief Servo controller peripheral using CRUMBS handler pattern.
 *
 * This peripheral demonstrates a position-control interface with:
 * - SET operations (0x01-0x03): Control servo positions, speed, and sweep
 * - GET operations (0x80-0x81): Query current positions and settings
 * - 2 servo motors on PWM pins D9-D10
 * - Speed-limited smooth movement
 * - Sweep patterns for automated testing
 *
 * Hardware:
 * - Arduino Nano (ATmega328P)
 * - I2C address: 0x30
 * - Servo 0: Pin 9 (D9)
 * - Servo 1: Pin 10 (D10)
 * - I2C pins: A4 (SDA), A5 (SCL)
 *
 * **IMPORTANT:** Servos draw significant current. Use external 5V power supply
 * for reliable operation, especially when controlling multiple servos.
 *
 * Operations:
 * - SERVO_OP_SET_POS (0x01): Set servo position (0-180 degrees)
 * - SERVO_OP_SET_SPEED (0x02): Set movement speed limit (0=instant, 1-20=slow)
 * - SERVO_OP_SWEEP (0x03): Configure automatic sweep pattern
 * - SERVO_OP_GET_POS (0x80): Returns current positions
 * - SERVO_OP_GET_SPEED (0x81): Returns speed limits
 */

#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

/* Include contract header (from parent directory) */
#include "servo_ops.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define PERIPHERAL_ADDR 0x30
#define NUM_SERVOS 2

/* Servo pins (PWM-capable on Arduino Nano) */
#define SERVO_PIN_0 9
#define SERVO_PIN_1 10

/* Update interval for smooth movement and sweep */
#define UPDATE_INTERVAL_MS 20

/* ============================================================================
 * Sweep Configuration Structure
 * ============================================================================ */

typedef struct
{
    uint8_t enabled;  /* 0=disabled, 1=enabled */
    uint8_t min_pos;  /* Minimum position (0-180) */
    uint8_t max_pos;  /* Maximum position (0-180) */
    uint8_t step;     /* Degrees per update */
    int8_t direction; /* 1=increasing, -1=decreasing */
} sweep_config_t;

/* ============================================================================
 * State
 * ============================================================================ */

static crumbs_context_t ctx;
static Servo g_servos[NUM_SERVOS];             /* Servo objects */
static uint8_t g_positions[NUM_SERVOS];        /* Current positions (0-180) */
static uint8_t g_target_positions[NUM_SERVOS]; /* Target positions (0-180) */
static uint8_t g_speeds[NUM_SERVOS];           /* Speed limits (0=instant, 1-20=slow) */
static sweep_config_t g_sweeps[NUM_SERVOS];    /* Sweep configurations */
static unsigned long g_last_update = 0;        /* Last update timestamp */

/* ============================================================================
 * Helper: Initialize Servos
 * ============================================================================ */

static void init_servos()
{
    const uint8_t pins[NUM_SERVOS] = {SERVO_PIN_0, SERVO_PIN_1};

    for (int i = 0; i < NUM_SERVOS; i++)
    {
        g_servos[i].attach(pins[i]);
        g_positions[i] = 90;
        g_target_positions[i] = 90;
        g_speeds[i] = 0; /* Instant movement by default */
        g_servos[i].write(90);

        /* Initialize sweep to disabled */
        g_sweeps[i].enabled = 0;
        g_sweeps[i].min_pos = 0;
        g_sweeps[i].max_pos = 180;
        g_sweeps[i].step = 1;
        g_sweeps[i].direction = 1;
    }
}

/* ============================================================================
 * Helper: Update Servo Positions (Smooth Movement)
 * ============================================================================ */

static void update_servos()
{
    for (int i = 0; i < NUM_SERVOS; i++)
    {
        /* Handle sweep if enabled */
        if (g_sweeps[i].enabled)
        {
            /* Calculate next position */
            int next_pos = g_positions[i] + (g_sweeps[i].step * g_sweeps[i].direction);

            /* Check bounds and reverse direction if needed */
            if (next_pos >= g_sweeps[i].max_pos)
            {
                next_pos = g_sweeps[i].max_pos;
                g_sweeps[i].direction = -1;
            }
            else if (next_pos <= g_sweeps[i].min_pos)
            {
                next_pos = g_sweeps[i].min_pos;
                g_sweeps[i].direction = 1;
            }

            g_target_positions[i] = next_pos;
        }

        /* Move towards target position */
        if (g_positions[i] != g_target_positions[i])
        {
            if (g_speeds[i] == 0)
            {
                /* Instant movement */
                g_positions[i] = g_target_positions[i];
            }
            else
            {
                /* Speed-limited movement */
                if (g_positions[i] < g_target_positions[i])
                {
                    g_positions[i] = min((int)g_positions[i] + g_speeds[i], (int)g_target_positions[i]);
                }
                else
                {
                    g_positions[i] = max((int)g_positions[i] - g_speeds[i], (int)g_target_positions[i]);
                }
            }

            /* Write to servo */
            g_servos[i].write(g_positions[i]);
        }
    }
}

/* ============================================================================
 * Handler: SET_POS (SERVO_OP_SET_POS)
 * ============================================================================ */

/**
 * @brief Handler for SET_POS operation - sets servo target position.
 */
static void handler_set_pos(crumbs_context_t *ctx, uint8_t opcode,
                            const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    if (data_len < 2)
    {
        Serial.println(F("SET_POS: Invalid payload"));
        return;
    }

    uint8_t servo_idx = data[0];
    uint8_t position = data[1];

    if (servo_idx >= NUM_SERVOS)
    {
        Serial.print(F("SET_POS: Invalid servo index "));
        Serial.println(servo_idx);
        return;
    }

    if (position > 180)
    {
        position = 180;
    }

    /* Disable sweep when manually setting position */
    g_sweeps[servo_idx].enabled = 0;

    g_target_positions[servo_idx] = position;

    Serial.print(F("SET_POS: Servo "));
    Serial.print(servo_idx);
    Serial.print(F(" target="));
    Serial.print(position);
    Serial.print(F(" (current="));
    Serial.print(g_positions[servo_idx]);
    Serial.print(F(", speed="));
    Serial.print(g_speeds[servo_idx]);
    Serial.println(F(")"));
}

/* ============================================================================
 * Handler: SET_SPEED (SERVO_OP_SET_SPEED)
 * ============================================================================ */

/**
 * @brief Handler for SET_SPEED operation - sets movement speed limit.
 */
static void handler_set_speed(crumbs_context_t *ctx, uint8_t opcode,
                              const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    if (data_len < 2)
    {
        Serial.println(F("SET_SPEED: Invalid payload"));
        return;
    }

    uint8_t servo_idx = data[0];
    uint8_t speed = data[1];

    if (servo_idx >= NUM_SERVOS)
    {
        Serial.print(F("SET_SPEED: Invalid servo index "));
        Serial.println(servo_idx);
        return;
    }

    if (speed > 20)
    {
        speed = 20;
    }

    g_speeds[servo_idx] = speed;

    Serial.print(F("SET_SPEED: Servo "));
    Serial.print(servo_idx);
    Serial.print(F(" speed="));
    Serial.print(speed);
    Serial.println(F(" (0=instant, 1-20=slow)"));
}

/* ============================================================================
 * Handler: SWEEP (SERVO_OP_SWEEP)
 * ============================================================================ */

/**
 * @brief Handler for SWEEP operation - configures automatic sweep pattern.
 */
static void handler_sweep(crumbs_context_t *ctx, uint8_t opcode,
                          const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    if (data_len < 5)
    {
        Serial.println(F("SWEEP: Invalid payload"));
        return;
    }

    uint8_t servo_idx = data[0];
    uint8_t enable = data[1];
    uint8_t min_pos = data[2];
    uint8_t max_pos = data[3];
    uint8_t step = data[4];

    if (servo_idx >= NUM_SERVOS)
    {
        Serial.print(F("SWEEP: Invalid servo index "));
        Serial.println(servo_idx);
        return;
    }

    /* Clamp values */
    if (min_pos > 180)
        min_pos = 180;
    if (max_pos > 180)
        max_pos = 180;
    if (min_pos > max_pos)
    {
        uint8_t tmp = min_pos;
        min_pos = max_pos;
        max_pos = tmp;
    }
    if (step == 0)
        step = 1;
    if (step > 20)
        step = 20;

    g_sweeps[servo_idx].enabled = enable ? 1 : 0;
    g_sweeps[servo_idx].min_pos = min_pos;
    g_sweeps[servo_idx].max_pos = max_pos;
    g_sweeps[servo_idx].step = step;

    /* Set initial direction based on current position */
    if (g_positions[servo_idx] <= min_pos)
    {
        g_sweeps[servo_idx].direction = 1;
    }
    else if (g_positions[servo_idx] >= max_pos)
    {
        g_sweeps[servo_idx].direction = -1;
    }

    Serial.print(F("SWEEP: Servo "));
    Serial.print(servo_idx);
    Serial.print(F(" "));
    Serial.print(enable ? F("ENABLED") : F("DISABLED"));
    Serial.print(F(" (min="));
    Serial.print(min_pos);
    Serial.print(F(", max="));
    Serial.print(max_pos);
    Serial.print(F(", step="));
    Serial.print(step);
    Serial.println(F(")"));
}

/* ============================================================================
 * Reply Handlers (GET operations via SET_REPLY)
 * ============================================================================ */

static void reply_handler_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_build_version_reply(reply, SERVO_TYPE_ID,
                               SERVO_MODULE_VER_MAJOR,
                               SERVO_MODULE_VER_MINOR,
                               SERVO_MODULE_VER_PATCH);
}

static void reply_handler_get_pos(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, SERVO_TYPE_ID, SERVO_OP_GET_POS);
    for (int i = 0; i < NUM_SERVOS; i++)
    {
        crumbs_msg_add_u8(reply, g_positions[i]);
    }
}

static void reply_handler_get_speed(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, SERVO_TYPE_ID, SERVO_OP_GET_SPEED);
    for (int i = 0; i < NUM_SERVOS; i++)
    {
        crumbs_msg_add_u8(reply, g_speeds[i]);
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

    Serial.println(F("\n=== CRUMBS Servo Controller Peripheral ==="));
    Serial.print(F("I2C Address: 0x"));
    Serial.println(PERIPHERAL_ADDR, HEX);
    Serial.print(F("Type ID: 0x"));
    Serial.println(SERVO_TYPE_ID, HEX);
    Serial.println(F("Servos: 2 (D9, D10)"));
    Serial.println(F("WARNING: Use external 5V power for servos!"));
    Serial.println();

    /* Initialize servos */
    init_servos();
    Serial.println(F("Servos initialized to 90 degrees"));

    /* Initialize CRUMBS peripheral */
    crumbs_arduino_init_peripheral(&ctx, PERIPHERAL_ADDR);
    crumbs_set_type_id(&ctx, SERVO_TYPE_ID); /* drop frames addressed to other families */

    /* Register SET operation handlers */
    crumbs_register_handler(&ctx, SERVO_OP_SET_POS, handler_set_pos, NULL);
    crumbs_register_handler(&ctx, SERVO_OP_SET_SPEED, handler_set_speed, NULL);
    crumbs_register_handler(&ctx, SERVO_OP_SWEEP, handler_sweep, NULL);

    /* Register GET operation reply handlers */
    crumbs_register_reply_handler(&ctx, 0,                 reply_handler_version,   nullptr);
    crumbs_register_reply_handler(&ctx, SERVO_OP_GET_POS,  reply_handler_get_pos,   nullptr);
    crumbs_register_reply_handler(&ctx, SERVO_OP_GET_SPEED, reply_handler_get_speed, nullptr);

    Serial.println(F("Servo controller peripheral ready!"));
    Serial.println(F("Waiting for I2C commands...\n"));

    g_last_update = millis();
}

void loop()
{
    unsigned long now = millis();

    /* Update servo positions at regular intervals */
    if (now - g_last_update >= UPDATE_INTERVAL_MS)
    {
        update_servos();
        g_last_update = now;
    }

    delay(1);
}

/**
 * @file main.cpp
 * @brief Calculator peripheral using CRUMBS handler pattern.
 *
 * This peripheral demonstrates a function-style interface with:
 * - SET operations (0x01-0x04): Execute arithmetic commands (ADD, SUB, MUL, DIV)
 * - GET operations (0x80-0x8D): Retrieve results and history via SET_REPLY
 * - Circular history buffer storing last 12 operations
 * - Pure software implementation (no hardware required)
 *
 * Hardware:
 * - Arduino Nano (ATmega328P)
 * - I2C address: 0x10
 * - I2C pins: A4 (SDA), A5 (SCL)
 *
 * Operations:
 * - CALC_OP_ADD (0x01): Add two u32 values
 * - CALC_OP_SUB (0x02): Subtract two u32 values
 * - CALC_OP_MUL (0x03): Multiply two u32 values
 * - CALC_OP_DIV (0x04): Divide two u32 values (division by zero returns 0xFFFFFFFF)
 * - CALC_OP_GET_RESULT (0x80): Returns last result
 * - CALC_OP_GET_HIST_META (0x81): Returns history metadata (count, write_pos)
 * - CALC_OP_GET_HIST_0..11 (0x82-0x8D): Returns specific history entry
 */

#include <Arduino.h>
#include <Wire.h>
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

/* Include contract header (from parent directory) */
#include "calculator_ops.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define PERIPHERAL_ADDR 0x10
#define HISTORY_SIZE CALC_HISTORY_SIZE

/* ============================================================================
 * State
 * ============================================================================ */

static crumbs_context_t ctx;
static uint32_t g_last_result = 0;                   /* Last calculation result */
static calc_hist_entry_t g_history[HISTORY_SIZE]; /* Circular history buffer */
static uint8_t g_history_count = 0;                  /* Valid entries (0-12) */
static uint8_t g_history_write_pos = 0;              /* Next write slot (0-11) */

/* ============================================================================
 * Helper: Append to History
 * ============================================================================ */

/**
 * @brief Append operation to circular history buffer.
 *
 * @param op     Operation name (4 bytes, e.g., "ADD\0")
 * @param a      First operand
 * @param b      Second operand
 * @param result Result of operation
 */
static void append_to_history(const char *op, uint32_t a, uint32_t b, uint32_t result)
{
    calc_hist_entry_t *entry = &g_history[g_history_write_pos];

    /* Copy operation name (ensure null-termination) */
    strncpy(entry->op, op, 4);
    entry->op[4] = '\0';

    /* Store operands and result */
    entry->a = a;
    entry->b = b;
    entry->result = result;

    /* Update circular buffer pointers */
    g_history_write_pos = (g_history_write_pos + 1) % HISTORY_SIZE;
    if (g_history_count < HISTORY_SIZE)
    {
        g_history_count++;
    }
}

/* ============================================================================
 * Handler: ADD (CALC_OP_ADD)
 * ============================================================================ */

/**
 * @brief Handler for ADD operation - adds two u32 values.
 */
static void handler_add(crumbs_context_t *ctx, uint8_t opcode,
                        const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    calc_operands_t v;
    if (calc_operands_unpack(data, data_len, &v) != 0)
    {
        return;
    }
    uint32_t a = v.a, b = v.b;

    /* Perform addition */
    uint32_t result = a + b;
    g_last_result = result;

    /* Append to history */
    append_to_history("ADD", a, b, result);
}

/* ============================================================================
 * Handler: SUB (CALC_OP_SUB)
 * ============================================================================ */

/**
 * @brief Handler for SUB operation - subtracts b from a.
 */
static void handler_sub(crumbs_context_t *ctx, uint8_t opcode,
                        const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    calc_operands_t v;
    if (calc_operands_unpack(data, data_len, &v) != 0)
    {
        return;
    }
    uint32_t a = v.a, b = v.b;

    /* Perform subtraction */
    uint32_t result = a - b;
    g_last_result = result;

    /* Append to history */
    append_to_history("SUB", a, b, result);
}

/* ============================================================================
 * Handler: MUL (CALC_OP_MUL)
 * ============================================================================ */

/**
 * @brief Handler for MUL operation - multiplies two u32 values.
 */
static void handler_mul(crumbs_context_t *ctx, uint8_t opcode,
                        const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    calc_operands_t v;
    if (calc_operands_unpack(data, data_len, &v) != 0)
    {
        return;
    }
    uint32_t a = v.a, b = v.b;

    /* Perform multiplication */
    uint32_t result = a * b;
    g_last_result = result;

    /* Append to history */
    append_to_history("MUL", a, b, result);
}

/* ============================================================================
 * Handler: DIV (CALC_OP_DIV)
 * ============================================================================ */

/**
 * @brief Handler for DIV operation - divides a by b (integer division).
 * Division by zero returns 0xFFFFFFFF.
 */
static void handler_div(crumbs_context_t *ctx, uint8_t opcode,
                        const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)user_data;

    calc_operands_t v;
    if (calc_operands_unpack(data, data_len, &v) != 0)
    {
        return;
    }
    uint32_t a = v.a, b = v.b;

    /* Perform division (handle division by zero) */
    uint32_t result;
    if (b == 0)
    {
        result = 0xFFFFFFFF;
        Serial.println("DIV: Error (div by zero)");
    }
    else
    {
        result = a / b;
    }

    g_last_result = result;

    /* Append to history */
    append_to_history("DIV", a, b, result);
}

/* ============================================================================
 * Reply Handlers (GET operations via SET_REPLY)
 * ============================================================================ */

static void reply_handler_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_build_version_reply(reply, CALC_TYPE_ID,
                               CALC_MODULE_VER_MAJOR,
                               CALC_MODULE_VER_MINOR,
                               CALC_MODULE_VER_PATCH);
}

static void reply_handler_get_result(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    calc_result_t r;
    r.result = g_last_result;
    crumbs_msg_init(reply, CALC_TYPE_ID, CALC_OP_GET_RESULT);
    calc_result_pack(reply, &r);
}

static void reply_handler_get_hist_meta(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    calc_hist_meta_t r;
    r.count = g_history_count;
    r.write_pos = g_history_write_pos;
    crumbs_msg_init(reply, CALC_TYPE_ID, CALC_OP_GET_HIST_META);
    calc_hist_meta_pack(reply, &r);
}

/*
 * History entry GET ops (CALC_OP_GET_HIST_0..11) are handled via on_request
 * fallback rather than individually-registered reply handlers. The reply
 * table has its own CRUMBS_MAX_HANDLERS slots (this project builds with 8),
 * so 3 + 12 reply handlers would not fit. The on_request callback is kept
 * for this fan-out case and illustrates the fallback model.
 */
static void on_request_hist(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    uint8_t entry_idx = (uint8_t)(ctx->requested_opcode - CALC_OP_GET_HIST_0);

    crumbs_msg_init(reply, CALC_TYPE_ID, ctx->requested_opcode);

    if (entry_idx < g_history_count)
    {
        calc_hist_entry_pack(reply, &g_history[entry_idx]);
    }
    /* else data_len stays 0 - empty reply for non-existent entry */
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

    Serial.println("\n=== CRUMBS Calculator Peripheral ===");
    Serial.print("I2C Address: 0x");
    Serial.println(PERIPHERAL_ADDR, HEX);
    Serial.println();

    /* Initialize CRUMBS context */
    crumbs_arduino_init_peripheral(&ctx, PERIPHERAL_ADDR);
    crumbs_set_type_id(&ctx, CALC_TYPE_ID); /* drop frames addressed to other families */

    /* Register SET operation handlers */
    crumbs_register_handler(&ctx, CALC_OP_ADD, handler_add, nullptr);
    crumbs_register_handler(&ctx, CALC_OP_SUB, handler_sub, nullptr);
    crumbs_register_handler(&ctx, CALC_OP_MUL, handler_mul, nullptr);
    crumbs_register_handler(&ctx, CALC_OP_DIV, handler_div, nullptr);

    /* Register GET operation reply handlers */
    crumbs_register_reply_handler(&ctx, 0,                   reply_handler_version,       nullptr);
    crumbs_register_reply_handler(&ctx, CALC_OP_GET_RESULT,  reply_handler_get_result,    nullptr);
    crumbs_register_reply_handler(&ctx, CALC_OP_GET_HIST_META, reply_handler_get_hist_meta, nullptr);

    /* History entry GETs fall through to on_request_hist (see comment above that function) */
    crumbs_set_callbacks(&ctx, nullptr, on_request_hist, nullptr);

    Serial.println("Ready");
}

void loop()
{
    /* I2C communication handled by Wire library interrupts */
    /* Just keep peripheral alive */
    delay(100);
}

/**
 * @file main.cpp
 * @brief Interactive mock device controller.
 *
 * This example demonstrates:
 * - Using helper functions from mock_ops.h
 * - Interactive serial command interface
 * - Controller-side command sending
 * - SET_REPLY pattern for queries
 *
 * Hardware:
 * - Arduino Nano (or compatible) as I2C controller
 * - Mock peripheral at address 0x10
 *
 * Serial Commands:
 * - help                    - Show available commands
 * - echo <hex bytes>        - Send echo data (e.g., "echo DE AD BE EF")
 * - heartbeat <ms>          - Set heartbeat period in milliseconds
 * - toggle                  - Toggle heartbeat enable/disable
 * - status                  - Query heartbeat status and period
 * - getecho                 - Query stored echo data
 * - info                    - Query device info
 * - scan                    - Scan I2C bus
 */

#include <Arduino.h>
#include <Wire.h>
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

/* Include contract header (from parent directory) */
#include "mock_ops.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define PERIPHERAL_ADDR 0x10
#define MAX_CMD_LEN 128

/* ============================================================================
 * State
 * ============================================================================ */

static crumbs_context_t ctx;
static crumbs_device_t  dev;
static char cmd_buffer[MAX_CMD_LEN];
static uint8_t cmd_len = 0;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Print help text.
 */
static void print_help()
{
    Serial.println("\n=== Mock Controller Commands ===");
    Serial.println("  help                   - Show this help");
    Serial.println("  echo <hex bytes>       - Send echo data (e.g., 'echo DE AD BE EF')");
    Serial.println("  heartbeat <ms>         - Set heartbeat period (e.g., 'heartbeat 1000')");
    Serial.println("  toggle                 - Toggle heartbeat enable/disable");
    Serial.println("  status                 - Query heartbeat status and period");
    Serial.println("  getecho                - Query stored echo data");
    Serial.println("  info                   - Query device info");
    Serial.println("  scan                   - Scan I2C bus");
    Serial.println();
}

/**
 * @brief Query peripheral and print result using the combined _get_* API.
 */
static int query_and_print(uint8_t query_op, const char *label)
{
    Serial.print(label);
    Serial.print(": ");

    if (query_op == MOCK_OP_GET_ECHO)
    {
        mock_echo_result_t result;
        int rc = mock_get_echo(&dev, &result);
        if (rc != 0)
        {
            Serial.print("Error: Failed to get echo (");
            Serial.print(rc);
            Serial.println(")");
            return rc;
        }
        for (uint8_t i = 0; i < result.len; i++)
        {
            if (result.data[i] >= 32 && result.data[i] < 127)
                Serial.write(result.data[i]);
            else
            {
                Serial.print("<0x");
                if (result.data[i] < 0x10)
                    Serial.print('0');
                Serial.print(result.data[i], HEX);
                Serial.print(">");
            }
        }
        Serial.println();
    }
    else if (query_op == MOCK_OP_GET_STATUS)
    {
        mock_status_result_t result;
        int rc = mock_get_status(&dev, &result);
        if (rc != 0)
        {
            Serial.print("Error: Failed to get status (");
            Serial.print(rc);
            Serial.println(")");
            return rc;
        }
        Serial.print("Heartbeat: ");
        Serial.print(result.state ? "ENABLED" : "DISABLED");
        Serial.print(", Period: ");
        Serial.print(result.period_ms);
        Serial.println(" ms");
    }
    else if (query_op == MOCK_OP_GET_INFO)
    {
        mock_info_result_t result;
        int rc = mock_get_info(&dev, &result);
        if (rc != 0)
        {
            Serial.print("Error: Failed to get info (");
            Serial.print(rc);
            Serial.println(")");
            return rc;
        }
        for (uint8_t i = 0; i < result.len; i++)
        {
            if (result.info[i] >= 32 && result.info[i] < 127)
                Serial.write((uint8_t)result.info[i]);
            else
            {
                Serial.print("<0x");
                if ((uint8_t)result.info[i] < 0x10)
                    Serial.print('0');
                Serial.print((uint8_t)result.info[i], HEX);
                Serial.print(">");
            }
        }
        Serial.println();
    }
    else
    {
        Serial.print("Error: Unknown query opcode 0x");
        Serial.println(query_op, HEX);
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

/**
 * @brief Handle 'echo' command.
 */
static void cmd_echo(const char *args)
{
    uint8_t data[27];
    uint8_t len = 0;

    /* Parse hex bytes from args */
    const char *p = args;
    while (*p && len < sizeof(data))
    {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;

        /* Parse hex byte */
        char hex[3] = {0};
        if (*p && ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')))
        {
            hex[0] = *p++;
        }
        if (*p && ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')))
        {
            hex[1] = *p++;
        }

        data[len++] = (uint8_t)strtol(hex, nullptr, 16);
    }

    if (len == 0)
    {
        Serial.println("Usage: echo <hex bytes> (e.g., 'echo DE AD BE EF')");
        return;
    }

    /* Send using helper function */
    int rc = mock_send_echo(&dev, data, len);
    if (rc != 0)
    {
        Serial.println("Error: Failed to send echo");
    }
}

/**
 * @brief Handle 'heartbeat' command.
 */
static void cmd_heartbeat(const char *args)
{
    /* Parse period in milliseconds */
    unsigned long period = strtoul(args, nullptr, 10);

    if (period > 65535)
    {
        Serial.println("Error: Period must be 0-65535 ms");
        return;
    }

    if (*args == '\0')
    {
        Serial.println("Usage: heartbeat <ms> (e.g., 'heartbeat 500')");
        return;
    }

    /* Send using helper function */
    int rc = mock_send_heartbeat(&dev, (uint16_t)period);
    if (rc != 0)
    {
        Serial.println("Error: Failed to send heartbeat command");
    }
}

/**
 * @brief Handle 'toggle' command.
 */
static void cmd_toggle()
{
    int rc = mock_send_toggle(&dev);
    if (rc != 0)
    {
        Serial.println("Error: Failed to send toggle");
    }
}

/**
 * @brief Handle 'scan' command.
 */
static void cmd_scan()
{
    Serial.println("Scanning I2C bus...");

    uint8_t found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.print("  Device at 0x");
            if (addr < 0x10)
                Serial.print('0');
            Serial.println(addr, HEX);
            found++;
        }
    }

    if (found == 0)
    {
        Serial.println("  No devices found");
    }
}

/* ============================================================================
 * Command Parser
 * ============================================================================ */

/**
 * @brief Parse and execute command from buffer.
 */
static void parse_command()
{
    /* Trim trailing whitespace */
    while (cmd_len > 0 && (cmd_buffer[cmd_len - 1] == ' ' || cmd_buffer[cmd_len - 1] == '\r' || cmd_buffer[cmd_len - 1] == '\n'))
    {
        cmd_len--;
    }
    cmd_buffer[cmd_len] = '\0';

    if (cmd_len == 0)
        return;

    /* Find first space to separate command from args */
    char *args = strchr(cmd_buffer, ' ');
    if (args)
    {
        *args = '\0'; /* Terminate command */
        args++;       /* Point to arguments */

        /* Skip leading spaces in args */
        while (*args == ' ')
            args++;
    }
    else
    {
        args = cmd_buffer + cmd_len; /* Empty args */
    }

    /* Execute command */
    if (strcmp(cmd_buffer, "help") == 0)
    {
        print_help();
    }
    else if (strcmp(cmd_buffer, "echo") == 0)
    {
        cmd_echo(args);
    }
    else if (strcmp(cmd_buffer, "heartbeat") == 0)
    {
        cmd_heartbeat(args);
    }
    else if (strcmp(cmd_buffer, "toggle") == 0)
    {
        cmd_toggle();
    }
    else if (strcmp(cmd_buffer, "status") == 0)
    {
        query_and_print(MOCK_OP_GET_STATUS, "Status");
    }
    else if (strcmp(cmd_buffer, "getecho") == 0)
    {
        query_and_print(MOCK_OP_GET_ECHO, "Echo data");
    }
    else if (strcmp(cmd_buffer, "info") == 0)
    {
        query_and_print(MOCK_OP_GET_INFO, "Device info");
    }
    else if (strcmp(cmd_buffer, "scan") == 0)
    {
        cmd_scan();
    }
    else
    {
        Serial.print("Unknown command: ");
        Serial.println(cmd_buffer);
        Serial.println("Type 'help' for available commands");
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

    Serial.println("\n=== CRUMBS Mock Controller ===");
    Serial.print("Target peripheral: 0x");
    if (PERIPHERAL_ADDR < 0x10)
        Serial.print('0');
    Serial.println(PERIPHERAL_ADDR, HEX);

    /* Initialize CRUMBS context */
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    /* Build bound device handle for mock peripheral */
    dev.ctx      = &ctx;
    dev.addr     = PERIPHERAL_ADDR;
    dev.write_fn = crumbs_arduino_wire_write;
    dev.read_fn  = crumbs_arduino_read;
    dev.delay_fn = crumbs_arduino_delay_us;
    dev.io       = NULL;

    /* Initialize I2C as controller */
    Wire.begin();
    Wire.setClock(100000);

    print_help();
    Serial.print("> ");
}

void loop()
{
    /* Read serial input */
    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\n' || c == '\r')
        {
            Serial.println(); /* Echo newline */
            parse_command();
            cmd_len = 0;
            Serial.print("> ");
        }
        else if (c == '\b' || c == 0x7F) /* Backspace */
        {
            if (cmd_len > 0)
            {
                cmd_len--;
                Serial.write('\b');
                Serial.write(' ');
                Serial.write('\b');
            }
        }
        else if (cmd_len < MAX_CMD_LEN - 1)
        {
            cmd_buffer[cmd_len++] = c;
            Serial.write(c); /* Echo character */
        }
    }
}

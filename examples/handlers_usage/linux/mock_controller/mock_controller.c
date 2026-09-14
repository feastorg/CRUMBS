/**
 * @file mock_controller.c
 * @brief Interactive Linux controller for mock device peripheral.
 *
 * This example demonstrates:
 * - Interactive command-line interface for CRUMBS controller
 * - Using helper functions from mock_ops.h
 * - SET_REPLY pattern for queries
 * - Real-world Linux I2C communication
 *
 * Hardware Setup:
 * - Linux SBC (Raspberry Pi, etc.) as controller
 * - Mock peripheral at I2C address 0x10 (PERIPHERAL_ADDR below)
 *
 * Build:
 *   mkdir -p build && cd build
 *   cmake ..
 *   cmake --build .
 *
 * Usage:
 *   ./crumbs_mock_linux_controller [i2c-device]
 *   ./crumbs_mock_linux_controller /dev/i2c-1
 *
 * Type 'help' at the prompt for available commands.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"
#include "crumbs_linux.h"
#include "crumbs_message_helpers.h"

/* Contract header from parent directory */
#include "mock_ops.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define PERIPHERAL_ADDR 0x10
#define MAX_CMD_LEN 256

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void print_help(void);
static void trim_whitespace(char *str);
static int query_and_print(crumbs_device_t *dev,
                           uint8_t query_op, const char *label);
static int cmd_scan(crumbs_device_t *dev);
static int cmd_echo(crumbs_device_t *dev, const char *args);
static int cmd_heartbeat(crumbs_device_t *dev, const char *args);
static int cmd_toggle(crumbs_device_t *dev);

/* ============================================================================
 * Help Text
 * ============================================================================ */

static void print_help(void)
{
    printf("\n");
    printf("=== Mock Controller Commands ===\n");
    printf("  help                          - Show this help\n");
    printf("  scan                          - Scan I2C bus for devices\n");
    printf("  echo <hex bytes>              - Send echo data (e.g., 'echo DE AD BE EF')\n");
    printf("  heartbeat <ms>                - Set heartbeat period in milliseconds\n");
    printf("  toggle                        - Toggle heartbeat enable/disable\n");
    printf("  status                        - Query heartbeat status and period\n");
    printf("  getecho                       - Query stored echo data\n");
    printf("  info                          - Query device info\n");
    printf("  quit, exit                    - Exit the program\n");
    printf("\n");
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Trim leading and trailing whitespace from a string in-place.
 */
static void trim_whitespace(char *str)
{
    if (!str || !*str)
        return;

    /* Trim leading */
    char *start = str;
    while (isspace((unsigned char)*start))
        start++;

    /* Trim trailing */
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';

    /* Shift if needed */
    if (start != str)
        memmove(str, start, strlen(start) + 1);
}

/**
 * @brief Query peripheral and print result using the combined _get_* API.
 */
static int query_and_print(crumbs_device_t *dev,
                           uint8_t query_op, const char *label)
{
    printf("%s: ", label);

    if (query_op == MOCK_OP_GET_ECHO)
    {
        mock_echo_result_t result;
        int rc = mock_get_echo(dev, &result);
        if (rc != 0)
        {
            fprintf(stderr, "Error: Failed to get echo (%d)\n", rc);
            return rc;
        }
        for (uint8_t i = 0; i < result.len; i++)
        {
            if (result.data[i] >= 32 && result.data[i] < 127)
                putchar(result.data[i]);
            else
                printf("<0x%02X>", result.data[i]);
        }
        printf("\n");
        if (result.len > 0)
        {
            printf("  Hex: ");
            for (uint8_t i = 0; i < result.len; i++)
                printf("%02X ", result.data[i]);
            printf("\n");
        }
    }
    else if (query_op == MOCK_OP_GET_STATUS)
    {
        mock_status_result_t result;
        int rc = mock_get_status(dev, &result);
        if (rc != 0)
        {
            fprintf(stderr, "Error: Failed to get status (%d)\n", rc);
            return rc;
        }
        printf("Heartbeat: %s, Period: %u ms\n",
               result.state ? "ENABLED" : "DISABLED", result.period_ms);
    }
    else if (query_op == MOCK_OP_GET_INFO)
    {
        mock_info_result_t result;
        int rc = mock_get_info(dev, &result);
        if (rc != 0)
        {
            fprintf(stderr, "Error: Failed to get info (%d)\n", rc);
            return rc;
        }
        for (uint8_t i = 0; i < result.len; i++)
        {
            if (result.info[i] >= 32 && result.info[i] < 127)
                putchar(result.info[i]);
            else
                printf("<0x%02X>", (uint8_t)result.info[i]);
        }
        printf("\n");
    }
    else
    {
        fprintf(stderr, "Error: Unknown query opcode 0x%02X\n", query_op);
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Command: scan
 * ============================================================================ */

static int cmd_scan(crumbs_device_t *dev)
{
    printf("Scanning for CRUMBS devices (0x03-0x77)...\n");

    uint8_t found[128];
    int n = crumbs_linux_scan_for_crumbs(
        dev->ctx, (crumbs_linux_i2c_t *)dev->io, 0x03, 0x77, 0, /* non-strict */
        found, sizeof(found), 25000);

    if (n < 0)
    {
        fprintf(stderr, "  ERROR: scan failed (%d)\n", n);
        return -1;
    }

    if (n == 0)
    {
        printf("  No CRUMBS devices found.\n");
    }
    else
    {
        printf("  Found %d device(s):\n", n);
        for (int i = 0; i < n; i++)
        {
            printf("    0x%02X", found[i]);
            if (found[i] == PERIPHERAL_ADDR)
                printf(" (Mock)");
            printf("\n");
        }
    }
    return 0;
}

/* ============================================================================
 * Command: echo
 * ============================================================================ */

static int cmd_echo(crumbs_device_t *dev, const char *args)
{
    uint8_t data[27];
    uint8_t len = 0;

    /* Parse hex bytes */
    const char *p = args;
    while (*p && len < sizeof(data))
    {
        /* Skip whitespace */
        while (isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;

        /* Parse hex byte */
        unsigned int byte;
        if (sscanf(p, "%2x", &byte) != 1)
        {
            fprintf(stderr, "Error: Invalid hex byte at '%s'\n", p);
            return -1;
        }
        data[len++] = (uint8_t)byte;

        /* Skip parsed bytes */
        while (*p && !isspace((unsigned char)*p))
            p++;
    }

    if (len == 0)
    {
        printf("Usage: echo <hex bytes>  (e.g., 'echo DE AD BE EF')\n");
        return -1;
    }

    /* Send using helper function */
    int rc = mock_send_echo(dev, data, len);
    if (rc == 0)
    {
        printf("OK: Sent echo data (%u bytes)\n", len);
    }
    else
    {
        fprintf(stderr, "Error: Failed to send echo (%d)\n", rc);
    }
    return rc;
}

/* ============================================================================
 * Command: heartbeat
 * ============================================================================ */

static int cmd_heartbeat(crumbs_device_t *dev, const char *args)
{
    if (!args || !*args)
    {
        printf("Usage: heartbeat <ms>  (e.g., 'heartbeat 500')\n");
        return -1;
    }

    /* Parse period in milliseconds */
    unsigned long period = strtoul(args, NULL, 10);

    if (period > 65535)
    {
        fprintf(stderr, "Error: Period must be 0-65535 ms\n");
        return -1;
    }

    /* Send using helper function */
    int rc = mock_send_heartbeat(dev, (uint16_t)period);
    if (rc == 0)
    {
        printf("OK: Set heartbeat period to %lu ms\n", period);
    }
    else
    {
        fprintf(stderr, "Error: Failed to send heartbeat command (%d)\n", rc);
    }
    return rc;
}

/* ============================================================================
 * Command: toggle
 * ============================================================================ */

static int cmd_toggle(crumbs_device_t *dev)
{
    /* Send using helper function */
    int rc = mock_send_toggle(dev);
    if (rc == 0)
    {
        printf("OK: Sent toggle command\n");
    }
    else
    {
        fprintf(stderr, "Error: Failed to send toggle (%d)\n", rc);
    }
    return rc;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv)
{
    const char *i2c_device = "/dev/i2c-1";
    if (argc > 1)
    {
        i2c_device = argv[1];
    }

    printf("=== CRUMBS Mock Controller (Linux) ===\n");
    printf("I2C device: %s\n", i2c_device);
    printf("Target peripheral: 0x%02X\n\n", PERIPHERAL_ADDR);

    /* Initialize CRUMBS context and open I2C device */
    crumbs_context_t ctx;
    crumbs_linux_i2c_t lw;

    int rc = crumbs_linux_init_controller(&ctx, &lw, i2c_device, 25000);
    if (rc != 0)
    {
        fprintf(stderr, "Error: Failed to initialize controller (rc=%d)\n", rc);
        fprintf(stderr, "Try: sudo chmod 666 %s\n", i2c_device);
        return 1;
    }

    /* Build bound device handle for mock peripheral */
    crumbs_device_t dev = {
        .ctx     = &ctx,
        .addr    = PERIPHERAL_ADDR,
        .write_fn = crumbs_linux_i2c_write,
        .read_fn  = crumbs_linux_read,
        .delay_fn = crumbs_linux_delay_us,
        .io      = &lw
    };

    print_help();

    /* Command loop */
    char line[MAX_CMD_LEN];
    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        trim_whitespace(line);
        if (strlen(line) == 0)
            continue;

        /* Parse command and arguments */
        char *args = strchr(line, ' ');
        if (args)
        {
            *args = '\0';
            args++;
            trim_whitespace(args);
        }
        else
        {
            args = line + strlen(line);
        }

        /* Execute command */
        if (strcmp(line, "help") == 0)
        {
            print_help();
        }
        else if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
        {
            break;
        }
        else if (strcmp(line, "scan") == 0)
        {
            cmd_scan(&dev);
        }
        else if (strcmp(line, "echo") == 0)
        {
            cmd_echo(&dev, args);
        }
        else if (strcmp(line, "heartbeat") == 0)
        {
            cmd_heartbeat(&dev, args);
        }
        else if (strcmp(line, "toggle") == 0)
        {
            cmd_toggle(&dev);
        }
        else if (strcmp(line, "status") == 0)
        {
            query_and_print(&dev, MOCK_OP_GET_STATUS, "Status");
        }
        else if (strcmp(line, "getecho") == 0)
        {
            query_and_print(&dev, MOCK_OP_GET_ECHO, "Echo data");
        }
        else if (strcmp(line, "info") == 0)
        {
            query_and_print(&dev, MOCK_OP_GET_INFO, "Device info");
        }
        else
        {
            printf("Unknown command: %s\n", line);
            printf("Type 'help' for available commands\n");
        }
    }

    printf("\nExiting...\n");
    crumbs_linux_close(&lw);
    return 0;
}

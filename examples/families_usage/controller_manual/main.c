/**
 * @file main.c
 * @brief Manual (preconfigured) controller for lhwit_family peripherals.
 *
 * This is a complete interactive application demonstrating standard CRUMBS usage:
 * - Preconfigured device list from config.h (typical production pattern)
 * - Supports multiple devices of same type
 * - Command sending via canonical *_ops.h helper functions
 * - SET_REPLY query pattern via *_get_*() wrapper functions
 * - Platform-specific delay via crumbs_linux_delay_us()
 *
 * Structure:
 * - Configuration: Device array in config.h {type_id, address}
 * - Commands: Use canonical helper functions from *_ops.h
 * - Queries: Use SET_REPLY pattern (2-step: query write + response read)
 * - UI: Interactive shell (application layer, not library code)
 *
 * Difference from controller_discovery:
 * - No scan command (addresses are preconfigured)
 * - Faster startup (no bus scan required)
 * - Deterministic device order
 *
 * Usage:
 *   ./controller_manual [/dev/i2c-1]
 *   > list
 *   > calculator 0 add 10 20
 *   > led 0 set_all 0x0F
 *   > servo 0 set_pos 0 90
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crumbs.h"
#include "crumbs_linux.h"
#include "crumbs_message_helpers.h"

#include "../lhwit_family/lhwit_ops.h"
#include "config.h"

/* ============================================================================
 * Configuration
 * ============================================================================ */

/* Device info with bound device handle */
typedef struct
{
    uint8_t          type_id;
    const char      *name;
    int              index;  /* Index among devices of same type */
    crumbs_device_t  dev;    /* Fully bound handle - populated at startup */
} device_info_t;

static device_info_t g_devices[16];
static int g_device_count = 0;

#define MAX_CMD_LEN 256

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void print_help(void);
static void cmd_list(void);
static void trim_whitespace(char *str);
static int resolve_device(uint8_t type_id, const char *args,
                          device_info_t **dev_out, const char **rest_out);
static int cmd_calculator(const crumbs_device_t *dev, const char *rest);
static int cmd_led(const crumbs_device_t *dev, const char *rest);
static int cmd_servo(const crumbs_device_t *dev, const char *rest);
static int cmd_display(const crumbs_device_t *dev, const char *rest);

/* ============================================================================
 * Help
 * ============================================================================ */

static void print_help(void)
{
    printf("\nLHWIT Manual Controller Commands\n");
    printf("=================================\n\n");
    printf("General:\n");
    printf("  help                              - Show this help\n");
    printf("  list                              - List configured devices\n");
    printf("  quit, exit                        - Exit\n\n");

    printf("Device Selection:\n");
    printf("  <type> <idx> <cmd> [args]         - Command to device by index\n");
    printf("  <type> @<addr> <cmd> [args]       - Command to device by address\n\n");

    printf("Calculator:\n");
    printf("  calculator 0 add <a> <b>          - Add\n");
    printf("  calculator 0 sub <a> <b>          - Subtract\n");
    printf("  calculator 0 mul <a> <b>          - Multiply\n");
    printf("  calculator 0 div <a> <b>          - Divide\n");
    printf("  calculator 0 result               - Get last result\n");
    printf("  calculator 0 history              - Show history\n\n");

    printf("LED:\n");
    printf("  led 0 set_all <mask>              - Set all LEDs (0x0F = all on)\n");
    printf("  led 0 set_one <idx> <0|1>         - Set single LED\n");
    printf("  led 0 blink <idx> <en> <ms>       - Configure blink\n");
    printf("  led 0 get_state                   - Get LED state\n");
    printf("  led 0 get_blink                   - Get blink config\n\n");

    printf("Servo:\n");
    printf("  servo 0 set_pos <idx> <angle>     - Set position (0-180deg)\n");
    printf("  servo 0 set_speed <idx> <speed>   - Set speed (0-20)\n");
    printf("  servo 0 sweep <i> <en> <min> <max> <step> - Configure sweep\n");
    printf("  servo 0 get_pos                   - Get positions\n");
    printf("  servo 0 get_speed                 - Get speeds\n\n");

    printf("Display:\n");
    printf("  display 0 set_number <num> <dec>  - Display number (dec=0-4)\n");
    printf("  display 0 set_brightness <level>  - Set brightness (0-10)\n");
    printf("  display 0 clear                   - Clear display\n");
    printf("  display 0 get_value               - Get current value\n\n");
}
/* ============================================================================
 * List Command
 * ============================================================================ */

static void cmd_list(void)
{
    if (g_device_count == 0)
    {
        printf("No devices configured.\n");
        return;
    }

    printf("\nConfigured Devices:\n");
    printf("-------------------\n");
    for (int i = 0; i < g_device_count; i++)
    {
        printf("[%d] %s at 0x%02X (Type 0x%02X, Index %d)\n",
               i, g_devices[i].name, g_devices[i].dev.addr,
               g_devices[i].type_id, g_devices[i].index);
    }
    printf("\n");
}

/* ============================================================================
 * Device Lookup
 * ============================================================================ */

/**
 * @brief Parse device selector and return the matching device_info_t.
 *
 * Accepts "@<hex_addr>" for direct address or "<idx>" for type-indexed lookup.
 *
 * @param type_id   Expected device type.
 * @param args      Args string starting at the selector token.
 * @param dev_out   Set to the matching device_info_t on success.
 * @param rest_out  Set to the remainder of args after the selector (points to subcmd).
 * @return 0 on success, -1 if not found or parse error.
 */
static int resolve_device(uint8_t type_id, const char *args,
                          device_info_t **dev_out, const char **rest_out)
{
    const char *cmd_start;

    if (*args == '@')
    {
        unsigned int addr_val;
        if (sscanf(args, "@%i", &addr_val) != 1)
            return -1;
        uint8_t target = (uint8_t)addr_val;
        cmd_start = strchr(args, ' ');
        if (!cmd_start)
            return -1;
        cmd_start++;
        for (int i = 0; i < g_device_count; i++)
        {
            if (g_devices[i].dev.addr == target)
            {
                *dev_out = &g_devices[i];
                while (*cmd_start && isspace((unsigned char)*cmd_start))
                    cmd_start++;
                *rest_out = cmd_start;
                return 0;
            }
        }
        printf("Device at 0x%02X not found (run 'list' to see devices)\n", target);
        return -1;
    }
    else
    {
        int idx;
        if (sscanf(args, "%d", &idx) != 1)
            return -1;
        int type_count = 0;
        cmd_start = strchr(args, ' ');
        if (!cmd_start)
            return -1;
        cmd_start++;
        for (int i = 0; i < g_device_count; i++)
        {
            if (g_devices[i].type_id == type_id)
            {
                if (type_count == idx)
                {
                    *dev_out = &g_devices[i];
                    while (*cmd_start && isspace((unsigned char)*cmd_start))
                        cmd_start++;
                    *rest_out = cmd_start;
                    return 0;
                }
                type_count++;
            }
        }
        printf("Device #%d of type 0x%02X not found (run 'list' to see devices)\n", idx, type_id);
        return -1;
    }
}
static void trim_whitespace(char *str)
{
    if (!str)
        return;
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (*start == '\0')
    {
        str[0] = '\0';
        return;
    }
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;
    size_t len = (size_t)(end - start + 1);
    memmove(str, start, len);
    str[len] = '\0';
}

/* ============================================================================
 * Calculator Commands
 * ============================================================================ */

static int cmd_calculator(const crumbs_device_t *dev, const char *rest)
{
    char subcmd[32];
    if (sscanf(rest, "%31s", subcmd) != 1)
    {
        printf("Usage: calculator <idx|@addr> <add|sub|mul|div|result|history> [args...]\n");
        return -1;
    }

    const char *op_rest = rest + strlen(subcmd);
    while (*op_rest && isspace((unsigned char)*op_rest))
        op_rest++;

    int rc;

    if (strcmp(subcmd, "add") == 0 || strcmp(subcmd, "sub") == 0 ||
        strcmp(subcmd, "mul") == 0 || strcmp(subcmd, "div") == 0)
    {
        uint32_t a, b;
        if (sscanf(op_rest, "%u %u", &a, &b) != 2)
        {
            printf("Usage: calculator <idx|@addr> %s <a> <b>\n", subcmd);
            return -1;
        }

        calc_operands_t v = {a, b};
        if (strcmp(subcmd, "add") == 0)
            rc = calc_send_add(dev, &v);
        else if (strcmp(subcmd, "sub") == 0)
            rc = calc_send_sub(dev, &v);
        else if (strcmp(subcmd, "mul") == 0)
            rc = calc_send_mul(dev, &v);
        else
            rc = calc_send_div(dev, &v);

        if (rc != 0)
        {
            fprintf(stderr, "ERROR: Failed to send (%d)\n", rc);
            return rc;
        }

        printf("OK: %s(%u, %u) sent to 0x%02X. Use 'calculator result' to get answer.\n",
               subcmd, a, b, dev->addr);
        return 0;
    }
    else if (strcmp(subcmd, "result") == 0)
    {
        calc_result_t res;
        rc = calc_get_result(dev, &res);
        if (rc != 0)
        {
            fprintf(stderr, "ERROR: calc_get_result failed (%d)\n", rc);
            return rc;
        }
        printf("Result: %u\n", res.result);
        return 0;
    }
    else if (strcmp(subcmd, "history") == 0)
    {
        calc_hist_meta_t meta;
        rc = calc_get_hist_meta(dev, &meta);
        if (rc != 0)
            return rc;

        printf("History: %u entries\n", meta.count);

        for (uint8_t i = 0; i < meta.count; i++)
        {
            calc_hist_entry_t entry;
            rc = calc_get_hist_entry(dev, i, &entry);
            if (rc != 0)
                continue;
            printf("  [%u] %s(%u, %u) = %u\n", i, entry.op, entry.a, entry.b, entry.result);
        }
        return 0;
    }

    printf("Unknown calculator command: %s\n", subcmd);
    return -1;
}

/* ============================================================================
 * LED Commands
 * ============================================================================ */

static int cmd_led(const crumbs_device_t *dev, const char *rest)
{
    char subcmd[32];
    if (sscanf(rest, "%31s", subcmd) != 1)
    {
        printf("Usage: led <idx|@addr> <set_all|set_one|blink|get_state|get_blink> [args...]\n");
        return -1;
    }

    const char *op_rest = rest + strlen(subcmd);
    while (*op_rest && isspace((unsigned char)*op_rest))
        op_rest++;

    int rc;

    if (strcmp(subcmd, "set_all") == 0)
    {
        unsigned int mask;
        if (sscanf(op_rest, "%i", &mask) != 1)
        {
            printf("Usage: led set_all <mask>\n");
            return -1;
        }

        led_set_all_t v = {(uint8_t)mask};
        rc = led_send_set_all(dev, &v);
        if (rc == 0)
            printf("OK: LEDs at 0x%02X set to 0x%02X\n", dev->addr, (uint8_t)mask);
        return rc;
    }
    else if (strcmp(subcmd, "set_one") == 0)
    {
        unsigned int idx, state;
        if (sscanf(op_rest, "%u %u", &idx, &state) != 2)
        {
            printf("Usage: led set_one <idx> <state>\n");
            return -1;
        }

        led_set_one_t v = {(uint8_t)idx, (uint8_t)state};
        rc = led_send_set_one(dev, &v);
        if (rc == 0)
            printf("OK: LED %u at 0x%02X set to %s\n", idx, dev->addr, state ? "ON" : "OFF");
        return rc;
    }
    else if (strcmp(subcmd, "blink") == 0)
    {
        unsigned int idx, enable;
        uint16_t period_ms;
        if (sscanf(op_rest, "%u %u %hu", &idx, &enable, &period_ms) != 3)
        {
            printf("Usage: led blink <idx> <enable> <period_ms>\n");
            return -1;
        }

        led_blink_t v = {(uint8_t)idx, (uint8_t)enable, period_ms};
        rc = led_send_blink(dev, &v);
        if (rc == 0)
            printf("OK: LED %u at 0x%02X blink %s\n", idx, dev->addr, enable ? "enabled" : "disabled");
        return rc;
    }
    else if (strcmp(subcmd, "get_state") == 0)
    {
        led_state_result_t res;
        rc = led_get_state(dev, &res);
        if (rc != 0)
            return rc;

        printf("LED state at 0x%02X: 0x%02X (", dev->addr, res.states);
        for (int i = 3; i >= 0; i--)
            printf("%d", (res.states >> i) & 1);
        printf(")\n");
        for (int i = 0; i < 4; i++)
            printf("  LED %d: %s\n", i, (res.states & (1 << i)) ? "ON" : "OFF");
        return 0;
    }
    else if (strcmp(subcmd, "get_blink") == 0)
    {
        led_blink_result_t res;
        rc = led_get_blink(dev, &res);
        if (rc != 0)
            return rc;

        for (int i = 0; i < 4; i++)
            printf("  LED %d: blink=%s, period=%ums\n", i,
                   res.enable[i] ? "ON" : "OFF", res.period_ms[i]);
        return 0;
    }

    printf("Unknown LED command: %s\n", subcmd);
    return -1;
}

/* ============================================================================
 * Servo Commands
 * ============================================================================ */

static int cmd_servo(const crumbs_device_t *dev, const char *rest)
{
    char subcmd[32];
    if (sscanf(rest, "%31s", subcmd) != 1)
    {
        printf("Usage: servo <idx|@addr> <set_pos|set_speed|sweep|get_pos|get_speed> [args...]\n");
        return -1;
    }

    const char *op_rest = rest + strlen(subcmd);
    while (*op_rest && isspace((unsigned char)*op_rest))
        op_rest++;

    int rc;

    if (strcmp(subcmd, "set_pos") == 0)
    {
        unsigned int idx, angle;
        if (sscanf(op_rest, "%u %u", &idx, &angle) != 2)
        {
            printf("Usage: servo set_pos <idx> <angle>\n");
            return -1;
        }

        servo_set_pos_t v = {(uint8_t)idx, (uint8_t)angle};
        rc = servo_send_set_pos(dev, &v);
        if (rc == 0)
            printf("OK: Servo %u at 0x%02X position set to %udeg\n", idx, dev->addr, angle);
        return rc;
    }
    else if (strcmp(subcmd, "set_speed") == 0)
    {
        unsigned int idx, speed;
        if (sscanf(op_rest, "%u %u", &idx, &speed) != 2)
        {
            printf("Usage: servo set_speed <idx> <speed>\n");
            return -1;
        }

        servo_set_speed_t v = {(uint8_t)idx, (uint8_t)speed};
        rc = servo_send_set_speed(dev, &v);
        if (rc == 0)
            printf("OK: Servo %u at 0x%02X speed set to %u\n", idx, dev->addr, speed);
        return rc;
    }
    else if (strcmp(subcmd, "sweep") == 0)
    {
        unsigned int idx, enable, min_pos, max_pos, step;
        if (sscanf(op_rest, "%u %u %u %u %u", &idx, &enable, &min_pos, &max_pos, &step) != 5)
        {
            printf("Usage: servo sweep <idx> <enable> <min> <max> <step>\n");
            return -1;
        }

        servo_sweep_t v = {(uint8_t)idx, (uint8_t)enable, (uint8_t)min_pos, (uint8_t)max_pos, (uint8_t)step};
        rc = servo_send_sweep(dev, &v);
        if (rc == 0)
            printf("OK: Servo %u at 0x%02X sweep %s\n", idx, dev->addr, enable ? "enabled" : "disabled");
        return rc;
    }
    else if (strcmp(subcmd, "get_pos") == 0)
    {
        servo_pos_result_t res;
        rc = servo_get_pos(dev, &res);
        if (rc != 0)
            return rc;

        printf("Servo positions at 0x%02X: [0]=%udeg, [1]=%udeg\n", dev->addr, res.pos0, res.pos1);
        return 0;
    }
    else if (strcmp(subcmd, "get_speed") == 0)
    {
        servo_speed_result_t res;
        rc = servo_get_speed(dev, &res);
        if (rc != 0)
            return rc;

        printf("Servo speeds at 0x%02X: [0]=%u, [1]=%u\n", dev->addr, res.speed0, res.speed1);
        return 0;
    }

    printf("Unknown servo command: %s\n", subcmd);
    return -1;
}

/* ============================================================================
 * Display Command
 * ============================================================================ */

static int cmd_display(const crumbs_device_t *dev, const char *rest)
{
    char subcmd[32];
    if (sscanf(rest, "%31s", subcmd) != 1)
    {
        printf("Usage: display <idx|@addr> <set_number|set_brightness|clear|get_value> [args...]\n");
        return -1;
    }

    const char *op_rest = rest + strlen(subcmd);
    while (*op_rest && isspace((unsigned char)*op_rest))
        op_rest++;

    int rc;

    if (strcmp(subcmd, "set_number") == 0)
    {
        unsigned int number, decimal_pos;
        if (sscanf(op_rest, "%u %u", &number, &decimal_pos) != 2)
        {
            printf("Usage: display set_number <number> <decimal_pos>\n");
            printf("  number: 0-9999\n");
            printf("  decimal_pos: 0=none, 1=digit1 (left), 2=digit2, 3=digit3, 4=digit4 (right)\n");
            return -1;
        }

        display_set_number_t v = {(uint16_t)number, (uint8_t)decimal_pos};
        rc = display_send_set_number(dev, &v);
        if (rc == 0)
            printf("OK: Display showing %u (decimal pos %u)\n", number, decimal_pos);
        return rc;
    }
    else if (strcmp(subcmd, "set_brightness") == 0)
    {
        unsigned int level;
        if (sscanf(op_rest, "%u", &level) != 1)
        {
            printf("Usage: display set_brightness <level>\n");
            printf("  level: 0-10 (0=off, 10=brightest)\n");
            return -1;
        }

        display_set_brightness_t v = {(uint8_t)level};
        rc = display_send_set_brightness(dev, &v);
        if (rc == 0)
            printf("OK: Brightness set to %u\n", level);
        return rc;
    }
    else if (strcmp(subcmd, "clear") == 0)
    {
        rc = display_send_clear(dev);
        if (rc == 0)
            printf("OK: Display cleared\n");
        return rc;
    }
    else if (strcmp(subcmd, "get_value") == 0)
    {
        display_value_result_t res;
        rc = display_get_value(dev, &res);
        if (rc != 0)
            return rc;

        printf("Display: number=%u, decimal=%u, brightness=%u\n",
               res.number, res.decimal_pos, res.brightness);
        return 0;
    }

    printf("Unknown display command: %s\n", subcmd);
    return -1;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[])
{
    const char *i2c_device = (argc > 1) ? argv[1] : "/dev/i2c-1";

    /* CRUMBS Pattern: Initialize controller
     * crumbs_linux_init_controller() - Initialize platform HAL (Linux/i2c-dev)
     * This internally calls crumbs_init() with CONTROLLER role
     * Returns platform handle used for all I2C operations
     */
    crumbs_context_t ctx;
    crumbs_linux_i2c_t lw;
    int rc = crumbs_linux_init_controller(&ctx, &lw, i2c_device, 100000);
    if (rc != 0)
    {
        fprintf(stderr, "ERROR: Failed to open I2C device '%s' (%d)\n", i2c_device, rc);
        fprintf(stderr, "       Try: sudo chmod 666 %s\n", i2c_device);
        return 1;
    }

    /* Load device configuration */
    g_device_count = 0;
    int type_counts[256] = {0};
    for (size_t i = 0; i < DEVICE_CONFIG_COUNT && g_device_count < 16; i++)
    {
        device_info_t *dev = &g_devices[g_device_count];
        dev->type_id      = DEVICE_CONFIG[i].type_id;
        dev->index        = type_counts[dev->type_id]++;
        dev->dev.ctx      = &ctx;
        dev->dev.addr     = DEVICE_CONFIG[i].addr;
        dev->dev.write_fn = crumbs_linux_i2c_write;
        dev->dev.read_fn  = crumbs_linux_read;
        dev->dev.delay_fn = crumbs_linux_delay_us;
        dev->dev.io       = (void *)&lw;

        if (dev->type_id == CALC_TYPE_ID)
            dev->name = "Calculator";
        else if (dev->type_id == LED_TYPE_ID)
            dev->name = "LED";
        else if (dev->type_id == SERVO_TYPE_ID)
            dev->name = "Servo";
        else if (dev->type_id == DISPLAY_TYPE_ID)
            dev->name = "Display";
        else
            dev->name = "Unknown";

        g_device_count++;
    }

    printf("\nLHWIT Manual Controller\n");
    printf("=======================\n");
    printf("I2C Device: %s\n", i2c_device);
    printf("Loaded %d device(s) from config.h\n", g_device_count);
    printf("\nType 'list' to see devices, 'help' for commands.\n\n");

    char line[MAX_CMD_LEN];
    while (1)
    {
        printf("lhwit> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        trim_whitespace(line);
        if (strlen(line) == 0)
            continue;

        char cmd[64];
        if (sscanf(line, "%63s", cmd) != 1)
            continue;

        const char *args = line + strlen(cmd);
        while (*args && isspace((unsigned char)*args))
            args++;

        if (strcmp(cmd, "help") == 0)
            print_help();
        else if (strcmp(cmd, "list") == 0)
            cmd_list();
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0)
        {
            printf("Goodbye!\n");
            break;
        }
        else if (strcmp(cmd, "calculator") == 0)
        {
            device_info_t *d;
            const char *rest;
            if (resolve_device(CALC_TYPE_ID, args, &d, &rest) == 0)
                cmd_calculator(&d->dev, rest);
        }
        else if (strcmp(cmd, "led") == 0)
        {
            device_info_t *d;
            const char *rest;
            if (resolve_device(LED_TYPE_ID, args, &d, &rest) == 0)
                cmd_led(&d->dev, rest);
        }
        else if (strcmp(cmd, "servo") == 0)
        {
            device_info_t *d;
            const char *rest;
            if (resolve_device(SERVO_TYPE_ID, args, &d, &rest) == 0)
                cmd_servo(&d->dev, rest);
        }
        else if (strcmp(cmd, "display") == 0)
        {
            device_info_t *d;
            const char *rest;
            if (resolve_device(DISPLAY_TYPE_ID, args, &d, &rest) == 0)
                cmd_display(&d->dev, rest);
        }
        else
            printf("Unknown command: %s (type 'help')\n", cmd);
    }

    crumbs_linux_close(&lw);
    return 0;
}

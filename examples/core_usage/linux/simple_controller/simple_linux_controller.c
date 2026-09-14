#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"
#include "crumbs_linux.h"

// Default address of slice peripheral
#define DEFAULT_SLICE_ADDR 0x08

static void usage(const char *prog)
{
    printf("Usage:\n"
           "  %s [i2c-device] [peripheral-addr]   send a test message "
           "(default /dev/i2c-1 0x%02X)\n"
           "  %s scan [strict]                    scan /dev/i2c-1 for CRUMBS devices\n",
           prog, DEFAULT_SLICE_ADDR, prog);
}

int main(int argc, char **argv)
{
    printf("CRUMBS Linux Controller Example\n");

    crumbs_context_t ctx;
    crumbs_linux_i2c_t lw;

    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        usage(argv[0]);
        return 0;
    }

    //----------------------------------------------------------------------
    // Parse arguments. "scan" as the first argument selects scan mode on the
    // default bus; otherwise the arguments are [i2c-device] [peripheral-addr].
    // Example: ./crumbs_simple_linux_controller /dev/i2c-1 0x08
    //----------------------------------------------------------------------
    const char *device_path = "/dev/i2c-1";
    uint8_t slice_addr = DEFAULT_SLICE_ADDR;
    int scan_mode = (argc >= 2 && strcmp(argv[1], "scan") == 0);

    if (!scan_mode)
    {
        if (argc >= 2 && argv[1] && argv[1][0] != '\0')
        {
            device_path = argv[1];
        }
        if (argc >= 3 && argv[2])
        {
            unsigned long val = strtoul(argv[2], NULL, 0);
            if (val <= 0x7F)
                slice_addr = (uint8_t)val;
        }
    }

    // Initialize the controller
    int rc = crumbs_linux_init_controller(&ctx, &lw, device_path, 25000);
    if (rc != 0)
    {
        /* lw_open_bus already reports the errno; add the path it was given. */
        fprintf(stderr, "ERROR: crumbs_linux_init_controller failed (%d) for device %s\n",
                rc, device_path);
        return 1;
    }

    // Scan mode: run a CRUMBS-specific scan and exit.
    if (scan_mode)
    {
        int strict = 0;
        if (argc >= 3 && strcmp(argv[2], "strict") == 0)
            strict = 1;

        printf("Running CRUMBS-specific scan (strict=%d)...\n", strict);
        uint8_t found[128];
        int n = crumbs_linux_scan_for_crumbs(
            &ctx, &lw, 0x03, 0x77, strict, found, sizeof(found), 25000);

        if (n < 0)
        {
            fprintf(stderr, "ERROR: scan failed (%d)\n", n);
            crumbs_linux_close(&lw);
            return 1;
        }

        printf("Found %d CRUMBS device(s):\n", n);
        for (int i = 0; i < n; ++i)
        {
            printf("  0x%02X\n", found[i]);
        }

        crumbs_linux_close(&lw);
        return 0;
    }

    //----------------------------------------------------------------------
    // Build a message to send to the Arduino slice
    //----------------------------------------------------------------------
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type_id = 1;
    msg.opcode = 1;

    // Example data payload (3 floats encoded as bytes)
    float values[] = {12.34f, 5.0f, 9.87f};
    msg.data_len = sizeof(values);
    memcpy(msg.data, values, sizeof(values));

    //----------------------------------------------------------------------
    // Send message to slice
    //----------------------------------------------------------------------
    printf("Sending message to slice (0x%02X) via %s...\n", slice_addr, device_path);

    rc = crumbs_controller_send(
        &ctx,
        slice_addr,
        &msg,
        crumbs_linux_i2c_write, // Linux HAL write adapter
        &lw                     // Linux I2C context
    );

    if (rc != 0)
    {
        fprintf(stderr, "ERROR: crumbs_controller_send failed (%d)\n", rc);
        crumbs_linux_close(&lw);
        return 1;
    }

    printf("Message sent (%u payload bytes).\n", msg.data_len);

    //----------------------------------------------------------------------
    // Request a reply (Arduino slice will respond in onRequest handler)
    //----------------------------------------------------------------------
    printf("Requesting reply from slice...\n");

    crumbs_message_t reply;
    rc = crumbs_linux_read_message(&lw, slice_addr, &ctx, &reply);

    if (rc != 0)
    {
        fprintf(stderr, "ERROR: Failed to read reply (%d)\n", rc);
        crumbs_linux_close(&lw);
        return 1;
    }

    //----------------------------------------------------------------------
    // Print reply
    //----------------------------------------------------------------------
    printf("Reply received:\n");
    printf("  type_id:       %u\n", reply.type_id);
    printf("  opcode:  %u\n", reply.opcode);
    printf("  data_len:      %u\n", reply.data_len);
    printf("  data:          ");

    for (int i = 0; i < reply.data_len; i++)
    {
        printf("0x%02X ", reply.data[i]);
    }

    printf("\n  crc8:          0x%02X\n", reply.crc8);

    printf("\nCRC Stats:\n");
    printf("  crc_error_count: %u\n", crumbs_get_crc_error_count(&ctx));
    printf("  last_crc_ok:     %d\n", crumbs_last_crc_ok(&ctx));

    //----------------------------------------------------------------------
    // Shutdown
    //----------------------------------------------------------------------
    crumbs_linux_close(&lw);
    printf("\nDone.\n");
    return 0;
}

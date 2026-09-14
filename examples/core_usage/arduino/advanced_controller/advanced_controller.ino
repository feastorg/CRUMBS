/**
 * @file simple_controller.ino
 * @brief CRUMBS Controller example sketch to send messages to a CRUMBS Slice.
 */

#include <crumbs.h>
#include <crumbs_arduino.h>

#include <Wire.h>
#include "config.h"

// Instantiate CRUMBS as Controller, set to true for Controller mode
static crumbs_context_t crumbsController; // C struct
static uint32_t last_heartbeat_ms = 0;
static bool led_state = false;

static void heartbeat_tick()
{
    const uint32_t now = millis();
    if ((uint32_t)(now - last_heartbeat_ms) < HEARTBEAT_INTERVAL_MS)
        return;

    last_heartbeat_ms = now;
    led_state = !led_state;
    digitalWrite(LED_BUILTIN, led_state ? HIGH : LOW);
}

// Initializes the Controller device, sets up serial communication, and provides usage instructions.
void setup()
{
    Serial.begin(SERIAL_BAUD); /**< Initialize serial communication at configured baud rate */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    while (!Serial)
    {
        delay(SERIAL_WAIT_MS); // Wait for Serial Monitor to open
    }

    crumbs_arduino_init_controller(&crumbsController);
    crumbs_reset_crc_stats(&crumbsController);

    Serial.println(F("Controller: CRC diagnostics reset."));

    Serial.println(F("Controller ready. Enter messages in the format:"));
    Serial.println(F("address,type_id,opcode,data0,data1,data2,data3,data4,data5,data6"));
    Serial.println(F("Example Serial Commands:"));
    Serial.println(F("   0x10,1,1,75.0,1.0,0.0,65.0,2.0,7.0,3.14")); // Example: address 0x10, type_id 1, opcode 1, data...
}

// Main loop that listens for serial input, parses commands, and sends crumbs_message_ts to the specified Slice.
void loop()
{
    heartbeat_tick();
    // Listen for serial input to send commands or request data
    handleSerialInput();
}

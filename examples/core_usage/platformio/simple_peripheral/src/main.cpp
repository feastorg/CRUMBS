/**
 * @file
 * @brief Small PlatformIO Arduino peripheral demo for CRUMBS.
 */

#include <Arduino.h>
#include <crumbs_arduino.h>

/* LED_BUILTIN is not defined on all boards (e.g. bare ESP32 dev modules) */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

crumbs_context_t per_ctx;

static void on_message(crumbs_context_t *ctx, const crumbs_message_t *m)
{
  Serial.print(F("Received message: type_id="));
  Serial.print(m->type_id);
  Serial.print(F(" cmd="));
  Serial.print(m->opcode);
  Serial.print(F(" data_len="));
  Serial.println(m->data_len);

  // Print payload bytes (hex)
  for (size_t i = 0; i < m->data_len; ++i)
  {
    Serial.print(F(" data["));
    Serial.print(i);
    Serial.print(F("]=0x"));
    if (m->data[i] < 0x10)
      Serial.print('0');
    Serial.print(m->data[i], HEX);
  }
  if (m->data_len > 0)
    Serial.println();
}

static void on_request(crumbs_context_t *ctx, crumbs_message_t *reply)
{
  // Provide a simple reply payload for demo purposes
  reply->type_id = 0x10;
  reply->opcode = 0x42; // arbitrary
  reply->data_len = 4;
  reply->data[0] = 0xDE;
  reply->data[1] = 0xAD;
  reply->data[2] = 0xBE;
  reply->data[3] = 0xEF;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(1);
  }
  Serial.println("CRUMBS PlatformIO peripheral (Nano) - init at address 0x10");

  crumbs_arduino_init_peripheral(&per_ctx, 0x10);
  crumbs_set_callbacks(&per_ctx, on_message, on_request, NULL);
}

void loop()
{
  // Peripheral work is interrupt-driven by Arduino Wire callbacks; use loop to blink
  digitalWrite(LED_BUILTIN, millis() % 1000 < 50);
  delay(50);
}

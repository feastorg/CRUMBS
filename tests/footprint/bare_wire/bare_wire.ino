// Baseline: a bare-Wire peripheral with the same shape as crumbs_min.
#include <Wire.h>
static volatile uint8_t rx[32];
static volatile uint8_t rx_len;
static void on_receive(int n) {
  rx_len = 0;
  while (Wire.available()) { uint8_t b = Wire.read(); if (rx_len < sizeof rx) rx[rx_len++] = b; }
}
static void on_request(void) {
  static const uint8_t reply[4] = {0x01, 0x00, 0x00, 0x00};
  Wire.write(reply, sizeof reply);
}
void setup() { Wire.begin(0x10); Wire.onReceive(on_receive); Wire.onRequest(on_request); }
void loop() {}

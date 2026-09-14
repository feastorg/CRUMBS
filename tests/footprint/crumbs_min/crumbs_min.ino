// Minimal CRUMBS peripheral: init + one reply handler, no Serial.
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>
static crumbs_context_t ctx;
static void reply_version(crumbs_context_t *c, crumbs_message_t *reply, void *u) {
  (void)c; (void)u; crumbs_build_version_reply(reply, 0x01, 1, 0, 0);
}
void setup() {
  crumbs_arduino_init_peripheral(&ctx, 0x10);
  crumbs_register_reply_handler(&ctx, 0x00, reply_version, NULL);
}
void loop() {}

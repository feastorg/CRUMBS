/* A host stand-in for the Arduino core, wide enough for
 * src/hal/arduino/crumbs_i2c_arduino.cpp: a clock that only advances when
 * the code under test delays, so a poll loop's timing is exact. */
#ifndef FAKE_ARDUINO_H
#define FAKE_ARDUINO_H

#include <stddef.h>
#include <stdint.h>

#define ARDUINO 10819
#define TWI_FREQ 100000L /* AVR-shaped: the HAL sets the clock when this exists */

unsigned long millis(void);
unsigned long micros(void);
void delayMicroseconds(unsigned int us);

/* Test control: the clock starts at 0 and moves only through these. */
void fake_arduino_reset_clock(void);
void fake_arduino_advance_us(unsigned long us);

#endif /* FAKE_ARDUINO_H */

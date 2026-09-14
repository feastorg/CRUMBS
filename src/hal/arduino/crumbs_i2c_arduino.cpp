/**
 * @file
 * @brief Arduino HAL implementation (Wire) for CRUMBS.
 */

#include <Arduino.h>
#include <Wire.h>

#include "crumbs_arduino.h"
#include "crumbs_message.h"

/** @brief Default Two-Wire (I2C) bus frequency used by Arduino HAL (100 kHz). */
#ifndef CRUMBS_DEFAULT_TWI_FREQ
#define CRUMBS_DEFAULT_TWI_FREQ 100000UL
#endif

#ifndef CRUMBS_ARDUINO_WIRE_BUFFER_LEN
#ifdef BUFFER_LENGTH
#define CRUMBS_ARDUINO_WIRE_BUFFER_LEN ((size_t)BUFFER_LENGTH)
#else
#define CRUMBS_ARDUINO_WIRE_BUFFER_LEN ((size_t)32u)
#endif
#endif

// Single global CRUMBS context pointer used by the Wire callbacks (single-bus pattern).
static crumbs_context_t *g_crumbs_ctx = nullptr;

/* ------------------------------------------------------------------------- */
/* Debug helpers (Arduino-specific)                                          */
/* ------------------------------------------------------------------------- */

#ifdef CRUMBS_DEBUG
#ifndef CRUMBS_DEBUG_PRINT
/* Default Arduino debug via Serial */
#define CRUMBS_ARDUINO_DBG_ENABLED 1
static void crumbs_arduino_dbg(const char *msg)
{
    Serial.print(F("[CRUMBS] "));
    Serial.println(msg);
}
static void crumbs_arduino_dbg_hex(const char *prefix, const uint8_t *data, size_t len)
{
    Serial.print(F("[CRUMBS] "));
    Serial.print(prefix);
    for (size_t i = 0; i < len && i < 8; i++)
    {
        if (data[i] < 0x10)
            Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    if (len > 8)
        Serial.print(F("..."));
    Serial.println();
}
#else
#define CRUMBS_ARDUINO_DBG_ENABLED 0
#endif
#else
#define CRUMBS_ARDUINO_DBG_ENABLED 0
#endif

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static void crumbs_arduino_on_receive(int numBytes)
{
    if (g_crumbs_ctx == nullptr || numBytes <= 0)
    {
#if CRUMBS_ARDUINO_DBG_ENABLED
        crumbs_arduino_dbg("on_receive: no ctx or 0 bytes");
#endif
        // Drain pending bytes to avoid leaving the Wire buffer full.
        while (Wire.available() > 0)
        {
            (void)Wire.read();
        }
        return;
    }

    uint8_t buffer[CRUMBS_MESSAGE_MAX_SIZE];
    size_t index = 0;

    while (Wire.available() > 0 && index < sizeof(buffer))
    {
        buffer[index++] = static_cast<uint8_t>(Wire.read());
    }

    // Drain any remaining bytes that do not fit in @buffer.
    while (Wire.available() > 0)
    {
        (void)Wire.read();
    }

#if CRUMBS_ARDUINO_DBG_ENABLED
    crumbs_arduino_dbg_hex("on_receive: ", buffer, index);
#endif

    // Pass bytes to the core decoder which will call on_message() when valid.
    int rc = crumbs_peripheral_handle_receive(g_crumbs_ctx, buffer, index);
#if CRUMBS_ARDUINO_DBG_ENABLED
    if (rc != 0)
    {
        Serial.print(F("[CRUMBS] on_receive: handle failed rc="));
        Serial.println(rc);
    }
#else
    (void)rc;
#endif
}

static void crumbs_arduino_on_request()
{
    if (g_crumbs_ctx == nullptr)
    {
#if CRUMBS_ARDUINO_DBG_ENABLED
        crumbs_arduino_dbg("on_request: no ctx");
#endif
        return;
    }

#if CRUMBS_ARDUINO_DBG_ENABLED
    crumbs_arduino_dbg("on_request: building reply");
#endif

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t frame_len = 0;

    int rc = crumbs_peripheral_build_reply(g_crumbs_ctx,
                                           frame,
                                           sizeof(frame),
                                           &frame_len);
    if (rc == 0 && frame_len > 0)
    {
#if CRUMBS_ARDUINO_DBG_ENABLED
        crumbs_arduino_dbg_hex("on_request: tx ", frame, frame_len);
#endif
        Wire.write(frame, frame_len);
    }
    else
    {
#if CRUMBS_ARDUINO_DBG_ENABLED
        Serial.print(F("[CRUMBS] on_request: no reply (rc="));
        Serial.print(rc);
        Serial.print(F(", len="));
        Serial.print(frame_len);
        Serial.println(F(")"));
#endif
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

extern "C" void crumbs_arduino_init_controller(crumbs_context_t *ctx)
{
    if (ctx == nullptr)
    {
        return;
    }

    // Initialize CRUMBS context as controller.
    crumbs_init(ctx, CRUMBS_ROLE_CONTROLLER, 0);

    // Use the default Wire bus.
    Wire.begin();
#if defined(TWI_FREQ) || defined(TWBR)
    // If the platform supports setClock(), use a standard frequency.
    Wire.setClock(CRUMBS_DEFAULT_TWI_FREQ);
#endif

    // Controller mode keeps Wire configured but does not register callbacks.
    // Users should call crumbs_controller_send() paired with crumbs_arduino_wire_write().
    g_crumbs_ctx = ctx;

#if CRUMBS_ARDUINO_DBG_ENABLED
    crumbs_arduino_dbg("init_controller: ready");
#endif
}

extern "C" void crumbs_arduino_init_peripheral(crumbs_context_t *ctx, uint8_t address)
{
    if (ctx == nullptr)
    {
        return;
    }

    // Initialize CRUMBS context as peripheral.
    crumbs_init(ctx, CRUMBS_ROLE_PERIPHERAL, address);

    // Configure TwoWire as an I2C slave at the given address.
    Wire.begin(address);
#if defined(TWI_FREQ) || defined(TWBR)
    Wire.setClock(CRUMBS_DEFAULT_TWI_FREQ);
#endif

    g_crumbs_ctx = ctx;

    // Register Wire callbacks that route into the CRUMBS core.
    Wire.onReceive(crumbs_arduino_on_receive);
    Wire.onRequest(crumbs_arduino_on_request);

#if CRUMBS_ARDUINO_DBG_ENABLED
    Serial.print(F("[CRUMBS] init_peripheral: addr=0x"));
    Serial.println(address, HEX);
#endif
}

extern "C" int crumbs_arduino_wire_write(void *user_ctx,
                                         uint8_t addr,
                                         const uint8_t *data,
                                         size_t len)
{
    // user_ctx is expected to be a TwoWire*; default to &Wire when null.
    TwoWire *wire = &Wire;
    if (user_ctx != nullptr)
    {
        wire = static_cast<TwoWire *>(user_ctx);
    }

    if (data == nullptr && len > 0)
    {
        return -1;
    }

    wire->beginTransmission(addr);
    size_t written = 0;
    if (len > 0)
    {
        written = wire->write(data, len);
    }
    uint8_t err = wire->endTransmission(); // 0 means success

    if (err != 0)
    {
        return (int)err; // propagate Wire error code
    }
    if (written != len)
    {
        return -2; // partial write
    }
    return 0;
}

extern "C" int crumbs_arduino_scan(void *user_ctx,
                                   uint8_t start_addr,
                                   uint8_t end_addr,
                                   int strict,
                                   uint8_t *found,
                                   size_t max_found)
{
    TwoWire *wire = &Wire;
    if (user_ctx != nullptr)
        wire = static_cast<TwoWire *>(user_ctx);

    if (found == nullptr || max_found == 0)
        return -1;

    size_t count = 0;
    for (int addr = start_addr; addr <= end_addr; ++addr)
    {
        bool present;
        if (strict)
        {
            // Strict probe: a one-byte read, as the Linux HAL does. Present if
            // the target ACKs its address for a read and clocks a byte out.
            // Puts no data on the bus, but does consume one byte from a
            // register-addressed or stream-style device.
            uint8_t got = wire->requestFrom((int)addr, (int)1); // (int,int) as elsewhere in this file
            while (wire->available())
                (void)wire->read();
            present = (got > 0);
        }
        else
        {
            // Non-strict probe: address-only write, no data phase. Present if
            // the target ACKs its address. Nothing is written to the device.
            wire->beginTransmission(static_cast<uint8_t>(addr));
            present = (wire->endTransmission() == 0);
        }

        if (present)
        {
            if (count < max_found)
                found[count] = (uint8_t)addr;
            ++count;
        }
    }

    return (int)count;
}

extern "C" int crumbs_arduino_read(void *user_ctx,
                                   uint8_t addr,
                                   uint8_t *buffer,
                                   size_t len,
                                   uint32_t timeout_us)
{
    TwoWire *wire = &Wire;
    if (user_ctx != nullptr)
        wire = static_cast<TwoWire *>(user_ctx);

    if (!buffer || len == 0)
        return -1;

    // Request up to @len bytes. Use a defensive timeout loop to avoid hangs.
    int requested = (int)len;
    /* Ensure we select the correct TwoWire overload on various cores by
       explicitly casting the length to int (Wire.requestFrom has variants
       taking int or uint8_t on different platforms). */
    /* Choose the int,int overload unambiguously on cores that provide
       multiple overloads by casting both args to int. */
#if ARDUINO >= 100
    (void)wire->requestFrom((int)static_cast<uint8_t>(addr), (int)requested);
#else
    (void)wire->requestFrom((int)addr, (int)requested);
#endif

    unsigned long start = micros();
    size_t idx = 0;

    while (idx < len)
    {
        if (wire->available())
        {
            buffer[idx++] = static_cast<uint8_t>(wire->read());
            continue;
        }

        if (timeout_us == 0u)
            break; /* no waiting */

        if ((unsigned long)(micros() - start) >= timeout_us)
            break; /* timed out */

        delayMicroseconds(50);
    }

    return (int)idx; /* number of bytes read (may be less than requested) */
}

extern "C" int crumbs_arduino_write_then_read(void *user_ctx,
                                              uint8_t addr,
                                              const uint8_t *tx,
                                              size_t tx_len,
                                              uint8_t *rx,
                                              size_t rx_len,
                                              uint32_t timeout_us,
                                              int require_repeated_start)
{
    TwoWire *wire = &Wire;
    if (user_ctx != nullptr)
        wire = static_cast<TwoWire *>(user_ctx);

    if ((tx_len > 0 && tx == nullptr) || (rx_len > 0 && rx == nullptr))
        return -1;

    if (tx_len > CRUMBS_ARDUINO_WIRE_BUFFER_LEN || rx_len > CRUMBS_ARDUINO_WIRE_BUFFER_LEN)
        return -6;

    if (rx_len == 0u)
    {
        if (tx_len == 0u)
            return 0;
        int wrc = crumbs_arduino_wire_write(wire, addr, tx, tx_len);
        if (wrc != 0)
            return -2;
        return 0;
    }

    if (tx_len > 0u)
    {
        wire->beginTransmission(addr);
        size_t written = wire->write(tx, tx_len);
        uint8_t err = wire->endTransmission(require_repeated_start ? 0 : 1);
        if (err != 0 || written != tx_len)
            return -2;
    }

#if ARDUINO >= 100
    (void)wire->requestFrom((int)addr, (int)rx_len);
#else
    (void)wire->requestFrom((int)addr, (int)rx_len);
#endif

    unsigned long start = micros();
    size_t idx = 0u;
    while (idx < rx_len)
    {
        if (wire->available())
        {
            rx[idx++] = static_cast<uint8_t>(wire->read());
            continue;
        }

        if (timeout_us == 0u)
            break;

        if ((unsigned long)(micros() - start) >= timeout_us)
            break;

        delayMicroseconds(50);
    }

    return (int)idx;
}

/**
 * @brief Arduino platform millisecond timer.
 * @return Milliseconds since boot.
 */
uint32_t crumbs_arduino_millis(void)
{
    return millis();
}

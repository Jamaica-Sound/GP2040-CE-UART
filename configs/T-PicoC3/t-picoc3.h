/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Board definition for LILYGO T-PicoC3
 *
 * RP2040 + ESP32-C3
 */

#ifndef _BOARDS_TPICOC3_H
#define _BOARDS_TPICOC3_H

// This header is also included by assembler.
// Keep it to preprocessor definitions only.

// -----------------------------------------------------------------------------
// Board identification
// -----------------------------------------------------------------------------

#define TPICOC3

// -----------------------------------------------------------------------------
// Platform
// -----------------------------------------------------------------------------

#define PICO_RP2040 1

// -----------------------------------------------------------------------------
// UART
//
// Internal RP2040 <-> ESP32-C3 connection:
//
// GPIO8 = UART1 TX
// GPIO9 = UART1 RX
// -----------------------------------------------------------------------------

#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 1
#endif

#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 8
#endif

#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 9
#endif

// -----------------------------------------------------------------------------
// LED
// -----------------------------------------------------------------------------

#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

// -----------------------------------------------------------------------------
// Flash
//
// T-PicoC3 uses a 4 MB SPI flash on the RP2040.
// Use the same W25Q080-compatible boot stage selection as the Pico family.
// -----------------------------------------------------------------------------

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

// -----------------------------------------------------------------------------
// RP2040 revision support
// -----------------------------------------------------------------------------

#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 1
#endif

// -----------------------------------------------------------------------------
// T-PicoC3 hardware pins
// -----------------------------------------------------------------------------

#define TPICOC3_TFT_RST_PIN 0
#define TPICOC3_TFT_DC_PIN 1
#define TPICOC3_TFT_SCLK_PIN 2
#define TPICOC3_TFT_MOSI_PIN 3
#define TPICOC3_TFT_BL_PIN 4
#define TPICOC3_TFT_CS_PIN 5

#define TPICOC3_BUTTON1_PIN 6
#define TPICOC3_BUTTON2_PIN 7

#define TPICOC3_UART_TX_PIN 8
#define TPICOC3_UART_RX_PIN 9

#define TPICOC3_PWR_ON_PIN 22

#define TPICOC3_LED_PIN 25

#define TPICOC3_BATTERY_ADC_PIN 26

#endif
/*
 * SPDX-License-Identifier: MIT
 *
 * GP2040-CE Board Configuration for LILYGO T-PicoC3
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "enums.pb.h"
#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "T-PicoC3"

// -----------------------------------------------------------------------------
// T-PicoC3 onboard buttons
//
// GPIO6 = onboard button 1
// GPIO7 = onboard button 2
//
// Buttons are active LOW on the T-PicoC3.
// -----------------------------------------------------------------------------

#define GPIO_PIN_06 GpioAction::BUTTON_PRESS_S1
#define GPIO_PIN_07 GpioAction::BUTTON_PRESS_S2

// -----------------------------------------------------------------------------
// Optional external inputs
//
// Add these only when you have actually wired them.
// GPIO8/9 MUST NOT be used for buttons:
// they are the RP2040 UART1 connection to the ESP32-C3.
// -----------------------------------------------------------------------------

// Example:
//
// #define GPIO_PIN_12 GpioAction::BUTTON_PRESS_UP
// #define GPIO_PIN_13 GpioAction::BUTTON_PRESS_DOWN
// ...

// -----------------------------------------------------------------------------
// Keyboard mapping
// -----------------------------------------------------------------------------

#define KEY_DPAD_UP       HID_KEY_ARROW_UP
#define KEY_DPAD_DOWN     HID_KEY_ARROW_DOWN
#define KEY_DPAD_RIGHT    HID_KEY_ARROW_RIGHT
#define KEY_DPAD_LEFT     HID_KEY_ARROW_LEFT

#define KEY_BUTTON_B1     HID_KEY_SHIFT_LEFT
#define KEY_BUTTON_B2     HID_KEY_Z

#define KEY_BUTTON_R2     HID_KEY_X
#define KEY_BUTTON_L2     HID_KEY_V
#define KEY_BUTTON_B3     HID_KEY_CONTROL_LEFT
#define KEY_BUTTON_B4     HID_KEY_ALT_LEFT

#define KEY_BUTTON_R1     HID_KEY_SPACE
#define KEY_BUTTON_L1     HID_KEY_C

#define KEY_BUTTON_S1     HID_KEY_5
#define KEY_BUTTON_S2     HID_KEY_1

#define KEY_BUTTON_L3     HID_KEY_EQUAL
#define KEY_BUTTON_R3     HID_KEY_MINUS

#define KEY_BUTTON_A1     HID_KEY_9
#define KEY_BUTTON_A2     HID_KEY_F2

#define KEY_BUTTON_FN     -1

#endif
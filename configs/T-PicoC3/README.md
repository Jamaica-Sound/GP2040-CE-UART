# GP2040 Configuration for LILYGO T-PicoC3

Board: LILYGO T-PicoC3

MCU:
- Raspberry Pi RP2040
- ESP32-C3

## RP2040 GPIO

| GPIO | Function |
|------|----------|
| 0 | TFT RESET |
| 1 | TFT DC |
| 2 | TFT SCLK |
| 3 | TFT MOSI |
| 4 | TFT Backlight |
| 5 | TFT CS |
| 6 | Button 1 / S1 |
| 7 | Button 2 / S2 |
| 8 | UART1 TX → ESP32-C3 |
| 9 | UART1 RX ← ESP32-C3 |
| 10 | ESP32-C3 control |
| 11 | ESP32-C3 control |
| 22 | Power ON |
| 25 | LED |
| 26 | Battery ADC |

## Important

GPIO8 and GPIO9 are reserved for the internal ESP32-C3 UART1 connection.

The UART input addon must use UART1 on GPIO8/9.

The board uses 4 MB flash.
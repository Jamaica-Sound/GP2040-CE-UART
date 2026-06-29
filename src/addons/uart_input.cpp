#include "addons/uart_input.h"
#include "storagemanager.h"
#include "gamepad.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <cstring>

#define UART_BOOT_GRACE_MS 1000
#define JSV2_SYNC 0x534A
#define JSV2_TYPE_CONFIG  0x1
#define JSV2_TYPE_RUNTIME 0x2

UartInputAddon* g_uartAddon = nullptr;

static void sendBytesBitBang(int pin, uint8_t b1, uint8_t b2, uint32_t baud);
static bool receiveJSBitBang(int pin, uint32_t baud, int timeoutMs);
static bool receiveSJBitBang(int pin, uint32_t baud, int timeoutMs);

static uint8_t runtimeDigitalCount = 0;
static uint8_t runtimeAnalogCount = 0;
static uint8_t runtimeDigitalPins[64];
static uint8_t runtimeAnalogPins[64];

// CRC table (precalcolata) – punto 7
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static uint16_t jsv2_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

static inline uint16_t jsv2_config_size(uint8_t digitalCount, uint8_t analogCount) {
    return 2 + 1 + 1 + 1 + digitalCount + analogCount + 2;
}
static inline uint16_t jsv2_runtime_size(uint8_t analogCount) {
    return 2 + 1 + 8 + (analogCount * sizeof(uint16_t)) + 2;
}

// ----------------------------------------------------------------------
//  UartInputAddon implementation
// ----------------------------------------------------------------------
UartInputAddon::UartInputAddon() {
    auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
    rxPin_ = opts.rxPin;
    txPin_ = opts.txPin;
    baudRate_ = opts.baudRate;
    g_uartAddon = this;
    handshakeStep_ = HS_IDLE;
    handshakeNeeded_ = false;
    handshakeStartTime_ = 0;
    handshakeStepStart_ = 0;
    bootGraceStart_ = to_ms_since_boot(get_absolute_time());
}

bool UartInputAddon::available() { return true; }
std::string UartInputAddon::name() { return "UART Input"; }

void UartInputAddon::setup() {
    auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
    if (!opts.enabled) {
        initialized = false;
        return;
    }
    rxPin_ = opts.rxPin;
    txPin_ = opts.txPin;
    bool pinsValid = (rxPin_ != -1 && txPin_ != -1);
    bool useDiscovery = pinsValid && !opts.handshake_done;
    baudRate_ = useDiscovery ? 9600 : opts.baudRate;

    if (pinsValid && !opts.handshake_done) {
        handshakeNeeded_ = true;
        handshakeStep_ = HS_IDLE;
        handshakeStartTime_ = 0;
        handshakeStepStart_ = 0;
        handshakeStatus_ = HandshakeStatus::IN_PROGRESS;
    } else if (pinsValid && opts.handshake_done) {
        handshakeNeeded_ = false;
        handshakeStatus_ = HandshakeStatus::SUCCESS;
    } else {
        handshakeNeeded_ = false;
        handshakeStatus_ = HandshakeStatus::IDLE;
        initialized = false;
        return;
    }

    uart_init(uart0, baudRate_);
    gpio_set_function(txPin_, GPIO_FUNC_UART);
    gpio_set_function(rxPin_, GPIO_FUNC_UART);
    uart_set_hw_flow(uart0, false, false);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
    rebuildVirtualGpioMap();
    initialized = true;
}

void UartInputAddon::rebuildVirtualGpioMap() {
    memset(virtualToGpio, 0xFF, sizeof(virtualToGpio));
    virtualOwnedMask = 0;
    virtualGpioMask = 0;
    auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
    for (int i = 0; i < opts.mappings_count; i++) {
        const auto& map = opts.mappings[i];
        if (map.virtualPin < UART_INPUT_MAX_VIRTUAL_PINS && map.gpio >= 0 && map.gpio < 30) {
            virtualToGpio[map.virtualPin] = (uint8_t)map.gpio;
            virtualOwnedMask |= (1UL << map.gpio);
        }
    }
}

void UartInputAddon::updateHandshake() {
    if (Storage::getInstance().getAddonOptions().uartOptions.handshake_done) {
        handshakeNeeded_ = false;
        handshakeStatus_ = HandshakeStatus::SUCCESS;
        return;
    }
    if (handshakeStatus_ == HandshakeStatus::SUCCESS) return;
    if (!handshakeNeeded_) return;

    unsigned long now = to_ms_since_boot(get_absolute_time());
    if ((now - bootGraceStart_) < UART_BOOT_GRACE_MS) return;

    if (txPin_ != -1 && rxPin_ != -1) {
        switch (handshakeStep_) {
            case HS_IDLE:
                handshakeStep_ = HS_MANUAL_SEND_TX;
                handshakeStepStart_ = now;
                break;
            case HS_MANUAL_SEND_TX:
                sendBytesBitBang(txPin_, 0x54, 0x58, baudRate_);
                handshakeStep_ = HS_MANUAL_SEND_RX;
                handshakeStepStart_ = now;
                break;
            case HS_MANUAL_SEND_RX: {
                uart_tx_wait_blocking(uart0);
                gpio_set_function(rxPin_, GPIO_FUNC_SIO);
                gpio_set_dir(rxPin_, GPIO_IN);
                gpio_pull_up(rxPin_);
                sleep_ms(2);
                while (uart_is_readable(uart0)) uart_getc(uart0);
                sendBytesBitBang(rxPin_, 0x52, 0x58, baudRate_);
                gpio_put(rxPin_, 1);
                sleep_ms(10);
                uart_tx_wait_blocking(uart0);
                while (uart_is_readable(uart0)) uart_getc(uart0);
                uart_deinit(uart0);
                sleep_ms(5);
                uart_init(uart0, baudRate_);
                gpio_set_function(txPin_, GPIO_FUNC_UART);
                gpio_set_function(rxPin_, GPIO_FUNC_UART);
                uart_set_hw_flow(uart0, false, false);
                uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
                uart_set_fifo_enabled(uart0, true);
                sleep_ms(2);
                handshakeStep_ = HS_MANUAL_LISTEN_OK;
                handshakeStepStart_ = now;
                break;
            }
            case HS_MANUAL_LISTEN_OK: {
               static char buf[32];
               static int pos = 0;
                while (uart_is_readable(uart0)) {
                  char c = uart_getc(uart0);
                   if (c == '\r') continue;
                   if (c == '\n') {
                       buf[pos] = '\0';
                      if (strstr(buf, "OK") != nullptr) {
                         pos = 0; memset(buf, 0, sizeof(buf));
                           handshakeStep_ = HS_SEND_FINAL;
                          handshakeStepStart_ = now;
                         break;
                        }
                   // --- AGGIUNTA: riconoscimento SKIP_DISCOVERY ---
                  if (strstr(buf, "SKIP_DISCOVERY") != nullptr) {
                       pos = 0; memset(buf, 0, sizeof(buf));
                       // Salta le fasi 1 e 2, vai direttamente all'handshake finale
                       handshakeStep_ = HS_SEND_FINAL;
                       handshakeStepStart_ = now;
                       break;
                    }
                // ------------------------------------------------
                  pos = 0; memset(buf, 0, sizeof(buf));
                  continue;
              }
                if (c >= 32 && c <= 126 && pos < (int)sizeof(buf)-1)
                  buf[pos++] = c;
                }
                 if ((now - handshakeStepStart_) > 500) {
                   pos = 0; memset(buf, 0, sizeof(buf));
                   handshakeStep_ = HS_MANUAL_SEND_TX;
                  handshakeStepStart_ = now;
               }
              break;
            }
            case HS_SEND_FINAL: {
                static absolute_time_t finalStart = 0;
                if (finalStart == 0) finalStart = get_absolute_time();
                bool ok = finalHandshakeUart0();
                if (ok) {
                    finalStart = 0;
                    handshakeStep_ = HS_DONE;
                    markHandshakeDone();
                    handshakeStatus_ = HandshakeStatus::SUCCESS;
                    handshakeNeeded_ = false;
                    uart_puts(uart0, "PICO READY\n");
                    return;
                }
                if (absolute_time_diff_us(finalStart, get_absolute_time()) > 4000000) {
                    finalStart = 0;
                    handshakeStep_ = HS_MANUAL_LISTEN_OK;
                    handshakeStepStart_ = now;
                }
                break;
            }
            default: break;
        }
        return;
    }

    // No specific tx/rx pins: use standard pattern handshake
    switch (handshakeStep_) {
        case HS_IDLE:
            handshakeStep_ = HS_SEND_JS;
            handshakeStepStart_ = now;
            break;
        case HS_SEND_JS:
            sendPatternJS();
            handshakeStep_ = HS_WAIT_SJ;
            handshakeStepStart_ = now;
            break;
        case HS_WAIT_SJ:
            if (receivePatternSJ(5)) {
                handshakeStep_ = HS_SEND_FINAL;
                handshakeStepStart_ = now;
            }
            if ((now - handshakeStepStart_) > 1000) {
                handshakeStep_ = HS_SEND_JS;
                handshakeStepStart_ = now;
            }
            break;
        case HS_SEND_FINAL:
            if (finalHandshakeUart0()) {
                handshakeStep_ = HS_DONE;
                markHandshakeDone();
                handshakeStatus_ = HandshakeStatus::SUCCESS;
                handshakeNeeded_ = false;
            }
            break;
        default: break;
    }
}

void UartInputAddon::preprocess() {}
void UartInputAddon::postprocess(bool sent) { (void)sent; }
void UartInputAddon::reinit() { initialized = false; setup(); }

void UartInputAddon::process() {
    if (handshakeNeeded_) {
        updateHandshake();
        return;
    }
    if (!initialized) return;
    if (handshakeStatus_ != HandshakeStatus::SUCCESS) return;

    const uint16_t bufMask = UART_RX_BUFFER_SIZE - 1; // punto 1

    while (uart_is_readable(uart0)) {
        uint16_t next = (rxHead + 1) & bufMask;
        if (next != rxTail) {
            rxBuffer[rxHead] = uart_getc(uart0);
            rxHead = next;
        } else {
            uart_getc(uart0);
        }
    }

    while (true) {
        uint16_t available = (rxHead + UART_RX_BUFFER_SIZE - rxTail) & bufMask;
        if (available < 1) return;
        uint16_t i = rxTail;
        if (available < 3) return;

        uint16_t sync = rxBuffer[i] | (rxBuffer[(i+1) & bufMask] << 8);
        if (sync != JSV2_SYNC) {
            rxTail = (rxTail + 1) & bufMask;
            continue;
        }

        uint8_t type = rxBuffer[(i+2) & bufMask];

        if (type == JSV2_TYPE_RUNTIME) {
            uint16_t needed = jsv2_runtime_size(runtimeAnalogCount);
            if (available < needed) return;

            // CRC check senza copia intermedia (punto 3 saltato, manteniamo copia per semplicità)
            uint8_t tmp[256];
            for (int k = 0; k < needed-2; k++)
                tmp[k] = rxBuffer[(i+k) & bufMask];
            uint16_t crc_calc = jsv2_crc16(tmp, needed-2);
            uint16_t crc_rx = rxBuffer[(i+needed-2) & bufMask] |
                               (rxBuffer[(i+needed-1) & bufMask] << 8);
            if (crc_calc != crc_rx) {
                rxTail = (rxTail + 1) & bufMask;
                continue;
            }

            uint16_t p = (i + 3) & bufMask;

            // digitalBits con memcpy (punto 6)
            uint64_t digitalBits = 0;
            if (p + 8 <= UART_RX_BUFFER_SIZE) {
                memcpy(&digitalBits, &rxBuffer[p], 8);
            } else {
                uint16_t firstPart = UART_RX_BUFFER_SIZE - p;
                memcpy(&digitalBits, &rxBuffer[p], firstPart);
                memcpy((uint8_t*)&digitalBits + firstPart, rxBuffer, 8 - firstPart);
            }
            p = (p + 8) & bufMask;

            // Scrittura diretta degli analogici (punto 4)
            for (int a = 0; a < runtimeAnalogCount; a++) {
                uint16_t idx = (p + a*2) & bufMask;
                uint16_t value = rxBuffer[idx] | (rxBuffer[(idx+1) & bufMask] << 8);
                uint8_t virtualPin = runtimeAnalogPins[a];
                if (virtualPin < UART_INPUT_MAX_VIRTUAL_PINS) {
                    uint8_t gpio = virtualToGpio[virtualPin];
                    if (gpio != 0xFF && gpio < 30)
                        virtualAnalogPinValues[gpio] = value;
                }
            }

            // Digitali (nessuna previousMask – punto 2)
            for (uint8_t i = 0; i < runtimeDigitalCount; i++) {
                uint64_t mask = (1ULL << i);
                uint8_t virtualPin = runtimeDigitalPins[i];
                bool pressed = (digitalBits & mask);
                if (virtualPin < UART_INPUT_MAX_VIRTUAL_PINS) {
                    uint8_t gpio = virtualToGpio[virtualPin];
                    if (gpio != 0xFF && gpio < 30) {
                        if (pressed) virtualGpioMask |= (1UL << gpio);
                        else virtualGpioMask &= ~(1UL << gpio);
                    }
                }
            }

            rxTail = (rxTail + jsv2_runtime_size(runtimeAnalogCount)) & bufMask;
            return; // punto 5 – esce dopo un pacchetto
        }

        if (type == JSV2_TYPE_CONFIG) {
            uint8_t digitalCount = rxBuffer[(i+3) & bufMask];
            uint8_t analogCount  = rxBuffer[(i+4) & bufMask];
            uint16_t needed = jsv2_config_size(digitalCount, analogCount);
            if (available < needed) return;

            uint16_t p = (i + 5) & bufMask;
            uint8_t tmp[256];
            for (int k = 0; k < needed-2; k++)
                tmp[k] = rxBuffer[(i+k) & bufMask];
            uint16_t crc_calc = jsv2_crc16(tmp, needed-2);
            uint16_t crc_rx = rxBuffer[(i+needed-2) & bufMask] |
                               (rxBuffer[(i+needed-1) & bufMask] << 8);
            if (crc_calc != crc_rx) {
                if (available > 64) rxTail = (rxTail + 8) & bufMask;
                else rxTail = (rxTail + needed) & bufMask;
                continue;
            }

            runtimeDigitalCount = digitalCount;
            runtimeAnalogCount  = analogCount;
            for (int k = 0; k < digitalCount; k++)
                runtimeDigitalPins[k] = rxBuffer[(p + k) & bufMask];
            p = (p + digitalCount) & bufMask;
            for (int k = 0; k < analogCount; k++)
                runtimeAnalogPins[k] = rxBuffer[(p + k) & bufMask];

            rxTail = (rxTail + needed) & bufMask;
            return; // punto 5 – esce dopo un pacchetto
        }
    }
}

// ----------------------------------------------------------------------
//  Static helper functions (bit‑bang, handshake patterns)
// ----------------------------------------------------------------------
static void sendBytesBitBang(int pin, uint8_t b1, uint8_t b2, uint32_t baud) {
    uint32_t bitTimeUs = 1000000 / baud;
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 1);
    sleep_us(bitTimeUs * 2);
    gpio_put(pin, 0);
    sleep_us(bitTimeUs);
    for (int i = 0; i < 8; i++) {
        gpio_put(pin, (b1 >> i) & 1);
        sleep_us(bitTimeUs);
    }
    gpio_put(pin, 1);
    sleep_us(bitTimeUs);
    gpio_put(pin, 0);
    sleep_us(bitTimeUs);
    for (int i = 0; i < 8; i++) {
        gpio_put(pin, (b2 >> i) & 1);
        sleep_us(bitTimeUs);
    }
    gpio_put(pin, 1);
    sleep_us(bitTimeUs * 2);
}

static bool receiveJSBitBang(int pin, uint32_t baud, int timeoutMs) {
    uint32_t bitTimeUs = 1000000 / baud;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    absolute_time_t start = get_absolute_time();
    while (absolute_time_diff_us(start, get_absolute_time()) < timeoutMs * 1000) {
        tight_loop_contents();
        if (gpio_get(pin) == 0) {
            sleep_us(bitTimeUs / 2);
            uint8_t data = 0;
            for (int i = 0; i < 8; i++) {
                sleep_us(bitTimeUs);
                data |= (gpio_get(pin) << i);
            }
            sleep_us(bitTimeUs);
            absolute_time_t subStart = get_absolute_time();
            while (absolute_time_diff_us(subStart, get_absolute_time()) < (timeoutMs * 1000 / 10)) {
                tight_loop_contents();
                if (gpio_get(pin) == 0) {
                    sleep_us(bitTimeUs / 2);
                    uint8_t data2 = 0;
                    for (int i = 0; i < 8; i++) {
                        sleep_us(bitTimeUs);
                        data2 |= (gpio_get(pin) << i);
                    }
                    sleep_us(bitTimeUs);
                    if (data == 0x4A && data2 == 0x53) return true;
                    break;
                }
            }
        }
    }
    return false;
}

static bool receiveSJBitBang(int pin, uint32_t baud, int timeoutMs) {
    uint32_t bitTimeUs = 1000000 / baud;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    absolute_time_t start = get_absolute_time();
    while (absolute_time_diff_us(start, get_absolute_time()) < timeoutMs * 1000) {
        tight_loop_contents();
        if (gpio_get(pin) == 0) {
            sleep_us(bitTimeUs / 2);
            uint8_t data = 0;
            for (int i = 0; i < 8; i++) {
                sleep_us(bitTimeUs);
                data |= (gpio_get(pin) << i);
            }
            sleep_us(bitTimeUs);
            absolute_time_t subStart = get_absolute_time();
            while (absolute_time_diff_us(subStart, get_absolute_time()) < (timeoutMs * 1000 / 10)) {
                tight_loop_contents();
                if (gpio_get(pin) == 0) {
                    sleep_us(bitTimeUs / 2);
                    uint8_t data2 = 0;
                    for (int i = 0; i < 8; i++) {
                        sleep_us(bitTimeUs);
                        data2 |= (gpio_get(pin) << i);
                    }
                    sleep_us(bitTimeUs);
                    if (data == 0x53 && data2 == 0x4A) return true;
                    break;
                }
            }
        }
    }
    return false;
}

// ----------------------------------------------------------------------
//  Handshake and public methods
// ----------------------------------------------------------------------
bool UartInputAddon::finalHandshakeUart0() {
    static bool phase1Sent = false;
    static bool phase2Sent = false;
    static bool baudSent = false;
    static char buf[32];
    static int pos = 0;
    static absolute_time_t phaseStart;
    static absolute_time_t baudSentTime = 0;
    static bool baudAckStarted = false;
    static absolute_time_t baudAckWindow;
    static bool baudAckSeen = false;

    if (!phase1Sent) {
        const char* msg = "handshake finale\n";
        for (const char* p = msg; *p; p++) uart_putc(uart0, *p);
        phaseStart = get_absolute_time();
        pos = 0; memset(buf, 0, sizeof(buf));
        phase1Sent = true;
        return false;
    }

    while (uart_is_readable(uart0)) {
        char c = uart_getc(uart0);
        if (c == '\n') { buf[pos] = '\0'; break; }
        if (c == '\r') continue;
        if (pos < (int)sizeof(buf)-1) buf[pos++] = c;
    }

    if (!phase2Sent) {
        if (strcmp(buf, "Handshake ok") == 0) {
            const char* msg2 = "trusted\n";
            for (const char* p = msg2; *p; p++) uart_putc(uart0, *p);
            uart_tx_wait_blocking(uart0);
            pos = 0; memset(buf, 0, sizeof(buf));
            phase2Sent = true;
            phaseStart = get_absolute_time();
            return false;
        }
        if (absolute_time_diff_us(phaseStart, get_absolute_time()) > 1000000) {
            phase1Sent = false; pos = 0; memset(buf, 0, sizeof(buf));
        }
        return false;
    }

    if (!baudSent) {
        if (strcmp(buf, "trusted") == 0) {
            auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
            char baudStr[16];
            snprintf(baudStr, sizeof(baudStr), "BAUD=%u\n", opts.baudRate);
            for (char* p = baudStr; *p; p++) uart_putc(uart0, *p);
            uart_tx_wait_blocking(uart0);
            pos = 0; memset(buf, 0, sizeof(buf));
            baudSent = true;
            phaseStart = get_absolute_time();
            baudSentTime = get_absolute_time();
            return false;
        }
        if (absolute_time_diff_us(phaseStart, get_absolute_time()) > 500000) {
            phase1Sent = false; phase2Sent = false; pos = 0; memset(buf, 0, sizeof(buf));
        }
        return false;
    }

    if (strcmp(buf, "baudrate received") == 0) {
        if (!baudAckStarted) {
            baudAckStarted = true;
            baudAckSeen = true;
            baudAckWindow = get_absolute_time();
        }
        pos = 0; memset(buf, 0, sizeof(buf));
        return false;
    }

    if (baudAckStarted) {
        if (absolute_time_diff_us(baudAckWindow, get_absolute_time()) > 500000) {
            if (baudAckSeen) {
                auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
                uint32_t finalBaud = opts.baudRate;
                uart_deinit(uart0);
                uart_init(uart0, finalBaud);
                gpio_set_function(txPin_, GPIO_FUNC_UART);
                gpio_set_function(rxPin_, GPIO_FUNC_UART);
                uart_set_hw_flow(uart0, false, false);
                uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
                uart_set_fifo_enabled(uart0, true);
                baudRate_ = finalBaud;
                phase1Sent = phase2Sent = baudSent = false;
                baudAckStarted = baudAckSeen = false;
                return true;
            }
        }
    }

    if (strcmp(buf, "baudrate unknown") == 0) {
        baudSent = false;
        pos = 0; memset(buf, 0, sizeof(buf));
        phaseStart = get_absolute_time();
        return false;
    }

    if (absolute_time_diff_us(phaseStart, get_absolute_time()) > 1000000) {
        phase1Sent = phase2Sent = baudSent = false;
        pos = 0; memset(buf, 0, sizeof(buf));
    }

    if (baudSent && !baudAckSeen) {
        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(baudSentTime, now) > 1000000) {
            auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
            opts.handshake_done = true;
            opts.rxPin = rxPin_;
            opts.txPin = txPin_;
            opts.baudRate = baudRate_;
            Storage::getInstance().save();
            handshakeStatus_ = HandshakeStatus::SUCCESS;
            handshakeNeeded_ = false;
            uart_puts(uart0, "PICO READY\n");
            phase1Sent = phase2Sent = baudSent = false;
            baudAckStarted = baudAckSeen = false;
            return true;
        }
    }
    return false;
}

bool UartInputAddon::finalHandshake() { return finalHandshakeUart0(); }

// punto 9 – rimozione parametri inutilizzati
bool UartInputAddon::sendPatternJS() {
    uart_putc(uart0, 0x4A);
    uart_putc(uart0, 0x53);
    return true;
}

bool UartInputAddon::receivePatternSJ(int timeoutMs) {
    absolute_time_t start = get_absolute_time();
    while (absolute_time_diff_us(start, get_absolute_time()) < timeoutMs * 1000) {
        tight_loop_contents();
        if (uart_is_readable(uart0)) {
            uint8_t b1 = uart_getc(uart0);
            if (uart_is_readable(uart0)) {
                uint8_t b2 = uart_getc(uart0);
                if (b1 == 0x53 && b2 == 0x4A) return true;
            }
        }
    }
    return false;
}

bool UartInputAddon::directHandshake() { return finalHandshake(); }

bool UartInputAddon::targetedDiscovery(uint32_t timeoutSeconds) {
    if (rxPin_ == -1 || txPin_ == -1) return false;
    handshakeStatus_ = HandshakeStatus::IN_PROGRESS;
    absolute_time_t start = get_absolute_time();
    uint64_t timeoutUs = timeoutSeconds * 1000000ULL;
    while (absolute_time_diff_us(start, get_absolute_time()) < (int64_t)timeoutUs) {
        tight_loop_contents();
        sendPatternJS();
        if (receivePatternSJ(5)) {
            if (finalHandshake()) {
                markHandshakeDone();
                return true;
            }
        }
        sleep_ms(1);
    }
    handshakeStatus_ = HandshakeStatus::TIMEOUT;
    return false;
}

bool UartInputAddon::autoDetect(uint32_t baudRate) {
    handshakeStatus_ = HandshakeStatus::IN_PROGRESS;
    absolute_time_t startTime = get_absolute_time();
    const uint64_t TIMEOUT_US = 30000000;
    int rxFound = -1;
    for (int pin : candidatePins_) {
        tight_loop_contents();
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        sleep_us(50);
        if (absolute_time_diff_us(startTime, get_absolute_time()) > (int64_t)TIMEOUT_US) break;
        if (receiveJSBitBang(pin, baudRate, 10)) { rxFound = pin; break; }
    }
    if (rxFound == -1) { handshakeStatus_ = HandshakeStatus::TIMEOUT; return false; }
    int txFound = -1;
    for (int pin : candidatePins_) {
        tight_loop_contents();
        if (pin == rxFound) continue;
        if (absolute_time_diff_us(startTime, get_absolute_time()) > (int64_t)TIMEOUT_US) break;
        sendPatternJS();
        if (receivePatternSJ(10)) { txFound = pin; break; }
    }
    if (txFound == -1) { handshakeStatus_ = HandshakeStatus::TIMEOUT; return false; }
    rxPin_ = rxFound;
    txPin_ = txFound;
    baudRate_ = baudRate;
    if (finalHandshake()) {
        auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
        opts.rxPin = rxPin_;
        opts.txPin = txPin_;
        opts.baudRate = baudRate_;
        markHandshakeDone();
        Storage::getInstance().save();
        handshakeStatus_ = HandshakeStatus::SUCCESS;
        return true;
    }
    handshakeStatus_ = HandshakeStatus::TIMEOUT;
    return false;
}

void UartInputAddon::markHandshakeDone() {
    auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
    opts.handshake_done = true;
    opts.rxPin = rxPin_;
    opts.txPin = txPin_;
    opts.baudRate = baudRate_;
    Storage::getInstance().save();
    handshakeStatus_ = HandshakeStatus::SUCCESS;
    handshakeNeeded_ = false;
}

void UartInputAddon::resetHandshakeFlag() {
    auto& opts = Storage::getInstance().getAddonOptions().uartOptions;
    opts.handshake_done = false;
    Storage::getInstance().save();
    handshakeStatus_ = HandshakeStatus::IDLE;
}

uint8_t UartInputAddon::getRuntimeAnalogCount() const {
    return runtimeAnalogCount;
}
#pragma once

#include "gpaddon.h"
#include "enums.pb.h"
#include "storagemanager.h"

#include <string>
#include <vector>

#define UART_INPUT_MAX_VIRTUAL_PINS 64

// limite locale del parser Pico
#define UART_INPUT_MAX_ANALOG 64

#define UART_RX_BUFFER_SIZE 1024 

enum class HandshakeStatus : uint8_t {
    IDLE,
    IN_PROGRESS,
    SUCCESS,
    TIMEOUT
};

class UartInputAddon : public GPAddon {
public:
    UartInputAddon();

    virtual bool available() override;
    virtual void setup() override;
    virtual void preprocess() override;
    virtual void process() override;
    virtual void postprocess(bool sent) override;
    virtual void reinit() override;
    virtual std::string name() override;

    bool autoDetect(uint32_t baudRate);
    bool directHandshake();

    HandshakeStatus getHandshakeStatus() const {
        return handshakeStatus_;
    }

    void resetHandshakeFlag();

    int getRxPin() const { return rxPin_; }
    int getTxPin() const { return txPin_; }

    bool isHandshakeDone() const {
        return Storage::getInstance()
            .getAddonOptions()
            .uartOptions
            .handshake_done;
    }

uint32_t getVirtualGpioMask() const {
    return virtualGpioMask;
}

uint32_t getVirtualOwnedMask() const {
    return virtualOwnedMask;
}

const uint16_t* getVirtualAnalogPinValues() const {
    return virtualAnalogPinValues;
}

const int8_t* getVirtualToGpioMap() const {
    return virtualToGpio;
}

uint8_t getRuntimeAnalogCount() const;


private:
    bool initialized = false;

    uint8_t rxBuffer[UART_RX_BUFFER_SIZE];

    volatile uint16_t rxHead = 0;
    volatile uint16_t rxTail = 0;

int8_t virtualToGpio[UART_INPUT_MAX_VIRTUAL_PINS];

// Runtime state for digital virtual pins
uint32_t virtualGpioMask;

// Runtime state for analog virtual pins
uint16_t virtualAnalogPinValues[UART_INPUT_MAX_ANALOG];

uint32_t virtualOwnedMask = 0;

uint8_t runtimeAnalogCount; 

    int rxPin_ = -1;
    int txPin_ = -1;

    uint32_t baudRate_ = 115200;

    HandshakeStatus handshakeStatus_ =
        HandshakeStatus::IDLE;

    enum HandshakeStep {
        HS_IDLE,
        HS_SEND_JS,
        HS_WAIT_SJ,
        HS_SEND_FINAL,
        HS_DONE,
        HS_FAILED,

        HS_MANUAL_SEND_TX,
        HS_MANUAL_SEND_RX,
        HS_MANUAL_LISTEN_OK
    };

    HandshakeStep handshakeStep_ = HS_IDLE;

    bool handshakeNeeded_ = false;

    unsigned long handshakeStartTime_ = 0;
    unsigned long handshakeStepStart_ = 0;

    uint32_t bootGraceStart_ = 0;

    const std::vector<int> candidatePins_ = {
        0,1,2,3,4,5,6,7,
        8,9,10,11,12,13,14,15,
        16,17,18,19,20,21,22,23,
        24,25,26,27,28,29
    };

    bool sendPatternJS();
    bool receivePatternSJ(int timeoutMs);

    bool finalHandshakeUart0();
    bool finalHandshake();

    bool targetedDiscovery(uint32_t timeoutSeconds = 30);

    void updateHandshake();

void rebuildVirtualGpioMap();

    void markHandshakeDone();
};

extern UartInputAddon* g_uartAddon;
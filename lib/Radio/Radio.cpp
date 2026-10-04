/*
 * cc1101-rangetest, a range and link quality tester for the TI CC1101 sub-GHz
 * transceiver on ESP32 boards.
 *
 * Copyright (C) 2026 Julian Friedrich
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This project is hosted on GitHub:
 *   https://github.com/fjulian79/cc1101-rangetest
 * Please feel free to file issues, open pull requests, or contribute there.
 */

#include "Radio.hpp"

#include <Arduino.h>
#include <RadioLib.h>

namespace DebugPins {
    constexpr uint8_t Tx = 13;
    constexpr uint8_t Rx = 12;
}

namespace RadioPins {
    constexpr uint8_t Cs =      27;
    constexpr uint8_t Sck =     18;
    constexpr uint8_t Miso =    19;
    constexpr uint8_t Mosi =    23;
    constexpr uint8_t Gdo0 =    26;
    constexpr uint8_t Gdo2 =    25;
}

ConfigFSK_t radioConfig = {
    .frequency = 433.92,
    .bitRate = 4.8,
    .frequencyDeviation = 5.0,
    .receiverBandwidth = 58.0,
    .power = 0,
    .preambleLength = 64
};

CC1101 cc1101 = new Module(
    RadioPins::Cs, RadioPins::Gdo0, RADIOLIB_NC, RadioPins::Gdo2);

volatile bool RXFlag;

ICACHE_RAM_ATTR void setRxFlag(void) {
  RXFlag = true;
}

Radio::Radio() : 
    lastRssi(0.0), 
    lastLqi(0) {
    
}

bool Radio::begin() {
    bool success = false;
    int16_t state = RADIOLIB_ERR_NONE;

    pinMode(RadioPins::Gdo0, INPUT);
    pinMode(RadioPins::Gdo2, INPUT);

    pinMode(DebugPins::Tx, OUTPUT);
    digitalWrite(DebugPins::Tx, LOW);
    pinMode(DebugPins::Rx, OUTPUT);
    digitalWrite(DebugPins::Rx, LOW);

    SPI.begin(RadioPins::Sck, RadioPins::Miso, RadioPins::Mosi, RadioPins::Cs);

    do {
        state = cc1101.begin(radioConfig);
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("Error: CC1101 initialization failed, state: %d\n", state);
            break;
        }

        // RadioLib leaves GDO0 high-impedance after initialization and readData().
        // Keep the IRQ input low in those intervals to avoid spurious RX events.
        // This has to be done after cc1101.begin(), which configures GDO0 as a plain INPUT.
        pinMode(RadioPins::Gdo0, INPUT_PULLDOWN);

        cc1101.setPacketReceivedAction(setRxFlag);
        state = cc1101.startReceive();
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("Error: CC1101 failed to start receiving, state: %d\n", state);
            break;
        }
    
        success = true;

    } while (0);

    return success;
}

bool Radio::transmit(const uint8_t* data, size_t len) {
    bool success = false;
    digitalWrite(DebugPins::Tx, HIGH);
    int16_t state = cc1101.transmit(data, len);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("Error: CC1101 failed to transmit, state: %d\n", state);
    } else {
        success = true;
    }
    digitalWrite(DebugPins::Tx, LOW);
    cc1101.startReceive();
    return success;
}

bool Radio::available(void) {
    return RXFlag;
}

size_t Radio::receive(uint8_t* buffer, size_t len) {
    int16_t state = RADIOLIB_ERR_NONE;
    size_t packetLength = 0;

    if (RXFlag) {
        digitalWrite(DebugPins::Rx, HIGH);
        RXFlag = false;
        packetLength = cc1101.getPacketLength();
        packetLength = (packetLength > len) ? len : packetLength;
        state = cc1101.readData(buffer, packetLength);
        if (state == RADIOLIB_ERR_NONE) {
            lastRssi = cc1101.getRSSI();
            lastLqi = cc1101.getLQI();
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.printf("Error: CC1101 CRC mismatch\n");
            packetLength = 0;
        } else {
            Serial.printf("Error: CC1101 failed to receive, state: %d\n", state);
            packetLength = 0;
        }
        cc1101.startReceive();
        digitalWrite(DebugPins::Rx, LOW);
    }

    return packetLength;
}

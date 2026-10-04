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

#pragma once

#include <Arduino.h>
#include <stdint.h>

constexpr uint8_t PacketMagic = 0xCC;
constexpr uint8_t PacketVersion = 1;
constexpr uint8_t PacketTypeProbe = 0x00;
constexpr uint8_t PacketTypeReply = 0x01;
constexpr uint8_t PacketTypeAck = 0x02;

struct __attribute__((packed)) RadioPacket {
    uint8_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t seqCnt;
    uint32_t uptime;
    int16_t rssi;
    uint8_t lqi;

    static RadioPacket probe(uint16_t seqCnt, uint32_t uptimeMs) {
        return RadioPacket{
            PacketMagic,
            PacketVersion,
            PacketTypeProbe,
            seqCnt,
            uptimeMs,
            0,
            0
        };
    }

    static RadioPacket reply(
        uint16_t seqCnt, uint32_t uptimeMs, int16_t rssiDbm, uint8_t lqi
    ) {
        return RadioPacket{
            PacketMagic,
            PacketVersion,
            PacketTypeReply,
            seqCnt,
            uptimeMs,
            rssiDbm,
            lqi
        };
    }

    static RadioPacket ack(
        uint16_t seqCnt, uint32_t uptimeMs, int16_t rssiDbm, uint8_t lqi
    ) {
        return RadioPacket{
            PacketMagic,
            PacketVersion,
            PacketTypeAck,
            seqCnt,
            uptimeMs,
            rssiDbm,
            lqi
        };
    }

    bool isValid() const {
        return magic == PacketMagic &&
               version == PacketVersion &&
               (type == PacketTypeProbe || type == PacketTypeReply || type == PacketTypeAck);
    }

    bool isProbe() const {
        return isValid() && type == PacketTypeProbe;
    }

    bool isReply() const {
        return isValid() && type == PacketTypeReply;
    }

    bool isAck() const {
        return isValid() && type == PacketTypeAck;
    }
};
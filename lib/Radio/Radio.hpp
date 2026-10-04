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


class Radio {

    public:

        Radio();

        bool begin();

        bool transmit(const uint8_t* data, size_t len);

        bool available(void);

        size_t receive(uint8_t* buffer, size_t len);

        float getRSSI(void) {
            return lastRssi;
        }
        
        uint8_t getLQI(void) {
            return lastLqi;
        }

    private:

        float lastRssi;
        
        uint8_t lastLqi;
};  
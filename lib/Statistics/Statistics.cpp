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

#include "Statistics.hpp"

Statistics::Statistics(size_t size) {
    dataSize = size < 1 ? 1 : size;
    pData = new DataPoint[dataSize];
    memset(pData, 0, sizeof(DataPoint) * dataSize);
    ready = false;
}

Statistics::~Statistics() {
    delete[] pData;
}

void Statistics::add(
    uint16_t seqCnt, int16_t roundTripTime, int16_t ownRssi, 
    uint8_t ownLqi, int16_t peerRssi, uint8_t peerLqi) 
{
    pData[index] = DataPoint{
        seqCnt, roundTripTime, ownRssi, ownLqi, peerRssi, peerLqi};
    incrementIndex();
}

void Statistics::addTimeout(uint16_t seqCnt) {
    pData[index] = DataPoint{seqCnt, -1, 0, 0, 0, 0};
    incrementIndex();
}

bool Statistics::isReady() const {
    return ready;
}

Statistics::Summary Statistics::get(void) const {
    Summary summary = {};
    int16_t lostPackets = 0;
    int16_t validPackets = 0;
    int32_t totalRoundTripTime = 0;
    int32_t totalOwnRssi = 0;
    int32_t totalPeerRssi = 0;
    int32_t totalOwnLqi = 0;
    int32_t totalPeerLqi = 0;

    if (!ready) {
        return summary;
    }

    for (size_t i = 0; i < dataSize; ++i) {
        const DataPoint &dp = pData[i];
        if (dp.roundTripTime < 0) {
            ++lostPackets;
        } else {
            ++validPackets;
            totalRoundTripTime += dp.roundTripTime;
            totalOwnRssi += dp.ownRssi;
            totalPeerRssi += dp.peerRssi;
            totalOwnLqi += dp.ownLqi;
            totalPeerLqi += dp.peerLqi;
        }
    }

    summary.packetLoss = lostPackets * 100 / (lostPackets + validPackets);
    summary.roundTripTime = validPackets ? totalRoundTripTime / validPackets : 0;
    summary.ownRssi = validPackets ? totalOwnRssi / validPackets : 0;
    summary.peerRssi = validPackets ? totalPeerRssi / validPackets : 0;
    summary.ownLqi = validPackets ? totalOwnLqi / validPackets : 0; // LQI averaging can be added if needed
    summary.peerLqi = validPackets ? totalPeerLqi / validPackets : 0; // LQI averaging can be added if needed

    return summary;
}
 
void Statistics::incrementIndex() {
    index++;
    if (index >= dataSize) {
        index = 0;
        ready = true;
    }
}

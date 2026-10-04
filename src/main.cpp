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

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <cli/cli.hpp>
#include <generic/generic.hpp>
#include <generic/task.hpp>
#include "version/version.h"
#include "Radio.hpp"
#include "Ping.hpp"
#include "Statistics.hpp"

#include <stdio.h>
#include <stdint.h>

namespace Oled {
    constexpr uint8_t Sda = 21;
    constexpr uint8_t Scl = 22;
    constexpr uint8_t Address = 0x3C;
    constexpr uint8_t Width = 128;
    constexpr uint8_t Height = 64;
}

Adafruit_SSD1306 display(Oled::Width, Oled::Height, &Wire, -1);

Cli cli;
Radio radio;
Statistics stats(10);

#ifdef CC1101_MOBILE

Task probeTask(300);

#endif

Task ledTask(250);
Task logTask(1000);
Task oledTask(500);

CLI_COMMAND(ver) {
    ioStream.printf("\n%s %s, Copyright (C) 2026 Julian Friedrich\n",
            VERSION_PROJECT, VERSION_GIT_SHORT);
    ioStream.printf("Build:           %s, %s\n", __DATE__, __TIME__);
    ioStream.printf("Git Repo:        %s\n", VERSION_GIT_REMOTE_ORIGIN);
    ioStream.printf("Revision:        %s\n", VERSION_GIT_LONG);
    ioStream.printf("\n");
    ioStream.printf("This program comes with ABSOLUTELY NO WARRANTY. This is free software, \n");
    ioStream.printf("and you are welcome to redistribute it under certain conditions.\n");
    ioStream.printf("See GPL v3 licence at https://www.gnu.org/licenses/ for details.\n\n");

    return 0;
}

CLI_COMMAND(stats) {
    Statistics::Summary summary = stats.get();
    ioStream.printf(
        "Packet Loss: %u%%, RTT: %d, Own RSSI: %d, Own LQI: %u, Peer RSSI: %d, Peer LQI: %u\n",
        summary.packetLoss,
        summary.roundTripTime,
        summary.ownRssi,
        summary.ownLqi,
        summary.peerRssi,
        summary.peerLqi
    );

    return 0;
}

CLI_COMMAND(log) {
    if (argc > 0 && strcmp(argv[0], "enable") == 0) {
        logTask.enable(true);
    } else if (argc > 0 && strcmp(argv[0], "disable") == 0) {
        logTask.enable(false);
    } else {
        goto out;
    }

    return 0;

    out:
    ioStream.printf("Usage: log <enable|disable>\n");
    return -1;
}

#ifdef CC1101_MOBILE

CLI_COMMAND(probe) {
    if (argc > 0 && strcmp(argv[0], "enable") == 0) {
        probeTask.enable(true);
    } else if (argc > 0 && strcmp(argv[0], "disable") == 0) {
        probeTask.enable(false);
    } else {
        ioStream.printf("Usage: probe <enable|disable>\n");
        return -1;
    }

    return 0;
}

#endif

CLI_COMMAND(reset) {
    ioStream.printf("Resetting the CPU ...\n");
    delay(100);
    ESP.restart();

    return 0;
}

CLI_COMMAND(help) {
    ioStream.printf("Available commands:\n");
    ioStream.printf("  help             Show this help\n");
    ioStream.printf("  ver              Show version information\n");
    ioStream.printf("  stats            Show link statistics\n");
    ioStream.printf("  log <enable|disable>\n");
    ioStream.printf("                   Enable/disable periodic statistics output\n");
#ifdef CC1101_MOBILE
    ioStream.printf("  probe <enable|disable>\n");
    ioStream.printf("                   Enable/disable sending probe packets\n");
#endif
    ioStream.printf("  reset            Reset CPU\n");
    ioStream.printf("\n");

    return 0;
}

bool oledPresent() {
    Wire.beginTransmission(Oled::Address);
    return Wire.endTransmission() == 0;
}

#ifdef CC1101_STATION

/**
 * The Station node radio implementation.
 */
void handleRadio(uint32_t now) {
    static uint16_t lastSeqCnt = 0;
    static uint32_t lastReplyTime = 0;
    static bool ackPending = false;

    if (radio.available()) {
        RadioPacket packet;
        size_t len = radio.receive((uint8_t*)&packet, sizeof(packet));
        if (len == sizeof(packet)) {
            int16_t rssi = (int16_t) radio.getRSSI();
            uint8_t lqi = radio.getLQI();

            if (packet.isProbe()) {
                RadioPacket reply = RadioPacket::reply(packet.seqCnt, now, rssi, lqi);
                radio.transmit((uint8_t*)&reply, sizeof(reply));
                if(ackPending) {
                    stats.addTimeout(lastSeqCnt);
                }
                lastSeqCnt = packet.seqCnt;
                lastReplyTime = now;
                ackPending = true;
            }
            else if (packet.isAck() && ackPending && packet.seqCnt == lastSeqCnt) {
                int16_t roundTripTime = now - lastReplyTime;
                ackPending = false;
                stats.add(packet.seqCnt, roundTripTime, rssi, lqi, packet.rssi, packet.lqi);
            }
            else {
                Serial.printf("Unexpected packet type received: type=%u, seq=%u\n", packet.type, packet.seqCnt);
            }
        } else {
            Serial.printf("Unexpected packet length received (len=%u)\n", (unsigned) len);
        }
    }
}

#else

/**
 * The Mobile node radio implementation.
 */
void handleRadio(uint32_t now) {
    static uint16_t seqCnt = 0;
    static uint32_t lastProbeTime = 0;
    static bool probePending = false;

    if (radio.available()) {
        RadioPacket packet;
        size_t len = radio.receive((uint8_t*)&packet, sizeof(packet));
        if (len == sizeof(packet)) {
            int16_t rssi = (int16_t) radio.getRSSI();
            uint8_t lqi = radio.getLQI();

            if (packet.isReply() && probePending && packet.seqCnt == (uint16_t)(seqCnt - 1)) {
                int16_t roundTripTime = now - lastProbeTime;
                RadioPacket ack = RadioPacket::ack(packet.seqCnt, now, rssi, lqi);
                radio.transmit((uint8_t*)&ack, sizeof(ack));
                probePending = false;
                stats.add( packet.seqCnt, roundTripTime, rssi, lqi, packet.rssi, packet.lqi);
            }             
            else {
                Serial.printf("Unexpected packet type received: type=%u, seq=%u\n", packet.type, packet.seqCnt);
            }
        } else {
            Serial.printf("Unexpected packet length received (len=%u)\n", (unsigned) len);
        }
    }

    if (probeTask.isScheduled(now)) {
        if (probePending) {
            stats.addTimeout(seqCnt - 1);
        }
        RadioPacket packet = RadioPacket::probe(seqCnt++, now);
        radio.transmit((uint8_t*)&packet, sizeof(packet));
        lastProbeTime = now;
        probePending = true;
    }

}

#endif

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    
    Serial.begin(115200);
    while (!Serial);
    Serial.println();
    cmd_ver(Serial, 0, 0);

    Wire.begin(Oled::Sda, Oled::Scl);
    if( oledPresent() && display.begin(SSD1306_SWITCHCAPVCC, Oled::Address)) {
        Serial.println("OLED initialized");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.printf("%s\n", VERSION_PROJECT);
        display.printf("%s\n", VERSION_GIT_SHORT);
        display.printf("%s\n", VERSION_DATE);
        display.printf("%s\n", VERSION_TIME);
        display.display();
    }
    else {
        Serial.println("No OLED present or initialization failed");
        oledTask.enable(false);
    }

    #ifdef CC1101_STATION
    printf("Station mode enabled\n");
    #else
    printf("Mobile mode enabled\n");
    logTask.enable(false);
    #endif


    radio.begin();
    cli.begin();
}

void loop() {
    uint32_t now = millis();

    handleRadio(now);

    if(ledTask.isScheduled(now)) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }

    if (logTask.isScheduled(now)) {
        Statistics::Summary summary = stats.get();
        Serial.printf(
            "Packet Loss: %u%%, RTT: %d, Own {RSSI: %d, LQI: %u}, Peer {RSSI: %d, LQI: %u}\n",
            summary.packetLoss, summary.roundTripTime,
            summary.ownRssi, summary.ownLqi,
            summary.peerRssi, summary.peerLqi
        );
    }

    if (oledTask.isScheduled(now)) {
        if (stats.isReady()) {
            Statistics::Summary summary = stats.get();
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println(VERSION_PROJECT);
            display.printf("Uptime: %lu s\n", millis() / 1000);
            display.printf("Loss: %u %%\n", summary.packetLoss);
            display.printf("RTT:  %d ms\n", summary.roundTripTime);
            display.printf("Own:  %d dBm %u\n", summary.ownRssi, summary.ownLqi);
            display.printf("Peer: %d dBm %u\n", summary.peerRssi, summary.peerLqi);
            display.display();
        }
    }

    cli.loop();
}

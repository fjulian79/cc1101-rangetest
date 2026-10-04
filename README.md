# cc1101-rangetest

A range and link quality tester for the TI CC1101 sub-GHz transceiver on ESP32
boards.
There are two nodes, a **station** and a **mobile**. They exchange ping packets
and measure packet loss, round-trip time, RSSI and LQI in both directions. The
results go to the serial console and, if one is connected, to an SSD1306 OLED.
That makes the mobile node usable as a handheld range tester.

Built with [PlatformIO](https://platformio.org/), the Arduino framework
([pioarduino](https://github.com/pioarduino/platform-espressif32)) and
[RadioLib](https://github.com/jgromes/RadioLib).

## How it works

The mobile node starts every exchange, and each exchange is a three-way
handshake:

```
mobile                         station
  |  ---- Probe (seq n) ---->    |   station measures RSSI/LQI of the probe
  |  <--- Reply (seq n) -----    |   reply carries the station's measurement
  |  ---- Ack   (seq n) ---->    |   ack carries the mobile's measurement
```

- **Mobile**: sends a probe every 300 ms. RTT is the time from probe to reply.
  If no matching reply arrives before the next probe, the exchange counts as
  lost.
- **Station**: answers every probe with a reply. RTT is the time from reply to
  ack. If no ack arrives before the next probe, the exchange counts as lost.

Both nodes therefore know the link quality in both directions:

| Value         | Meaning                                                 |
|---------------|---------------------------------------------------------|
| Own RSSI/LQI  | measured locally on the last packet received            |
| Peer RSSI/LQI | measured by the other node and sent back in the packet  |
| Loss          | lost exchanges over the last 10 exchanges, in percent   |
| RTT           | average round-trip time of the successful exchanges, ms |

On the CC1101, a **lower** LQI value means a **better** link.

### Packet format

All packets share one 12-byte packed structure (`lib/Ping/Ping.hpp`), sent
little-endian:

| Offset | Size | Field     | Description                              |
|--------|------|-----------|------------------------------------------|
| 0      | 1    | `magic`   | always `0xCC`                            |
| 1      | 1    | `version` | protocol version, currently `1`          |
| 2      | 1    | `type`    | `0` = probe, `1` = reply, `2` = ack      |
| 3      | 2    | `seqCnt`  | sequence number, set by the mobile       |
| 5      | 4    | `uptime`  | sender uptime in ms                      |
| 9      | 2    | `rssi`    | RSSI in dBm measured by the sender       |
| 11     | 1    | `lqi`     | LQI measured by the sender               |

The CC1101 adds sync word, length byte and CRC in hardware. Packets with a bad
CRC, the wrong length or an unknown magic/version/type are dropped.

### Radio settings

Set in `lib/Radio/Radio.cpp`:

| Parameter           | Value      |
|---------------------|------------|
| Frequency           | 433.92 MHz |
| Modulation          | 2-FSK      |
| Bit rate            | 4.8 kbps   |
| Frequency deviation | 5 kHz      |
| RX bandwidth        | 58 kHz     |
| TX power            | 0 dBm      |
| Preamble            | 64 bits    |

Both nodes must use the same settings. Check the regulations for your region
before you change frequency or power.

## Hardware

- 2× ESP32 board. The `station` env targets a NodeMCU-32S and the `mobile` env
  a LOLIN D32. Other ESP32 boards work if you change `board` in
  `platformio.ini`.
- 2× CC1101 module with a matching 433 MHz antenna
- Optional: SSD1306 OLED, 128×64, I²C

### Wiring

| CC1101 | ESP32 GPIO |
|--------|------------|
| CSN    | 27         |
| SCK    | 18         |
| MISO   | 19         |
| MOSI   | 23         |
| GDO0   | 26         |
| GDO2   | 25         |
| VCC    | 3.3 V      |
| GND    | GND        |

| OLED | ESP32 GPIO |
|------|------------|
| SDA  | 21         |
| SCL  | 22         |

The OLED is detected at I²C address `0x3C` on startup. If none is found, the
firmware just skips the display.

GPIO 13 (TX) and GPIO 12 (RX) are debug outputs. They go high while a packet is
transmitted or read, so you can follow the timing on a logic analyzer or
scope.

## Build and flash

```sh
# station
pio run -e station -t upload

# mobile
pio run -e mobile -t upload
```

`station` is the default env, so plain `pio run` only builds the station.

## Serial console

The console runs at 115200 baud and uses
[libCli](https://github.com/fjulian79/libCli), which echoes and edits the line
itself (history with ↑/↓, tab completion). The terminal therefore must not echo
locally and should send CR on Enter. `platformio.ini` is set up this way:

```sh
pio device monitor -e station
```

picocom works too:

```sh
picocom -b 115200 --imap lfcrlf --emap crcrlf,delbs /dev/tty.usbserial-0001
```

The `esp32_exception_decoder` filter buffers output until a newline, which
hides the prompt. Only use it when you need to decode a crash:

```sh
pio device monitor -f esp32_exception_decoder
```

### Commands

| Command                   | Description                                        |
|---------------------------|----------------------------------------------------|
| `help`                    | list the commands                                  |
| `ver`                     | show version and build information                 |
| `stats`                   | print the current link statistics once             |
| `log <enable\|disable>`   | print the statistics every second                  |
| `probe <enable\|disable>` | start or stop sending probes (mobile only)         |
| `reset`                   | restart the ESP32                                  |

Periodic logging is on by default on the station and off on the mobile, where
the OLED is usually the display of choice.

Example output (the values are only illustrative):

```
Packet Loss: 0%, RTT: 52, Own RSSI: -48, Own LQI: 3, Peer RSSI: -51, Peer LQI: 4
```

## Project layout

```
src/main.cpp          CLI commands, role-specific radio handling, OLED
lib/Radio/            thin RadioLib wrapper for the CC1101 (IRQ driven RX)
lib/Ping/             packet definition
lib/Statistics/       ring buffer of the last exchanges and their averages
doc/                  notes and patches (RadioLib GDO0 fix, libCli tab completion)
```

## License

GPL v3. See [LICENSE.txt](LICENSE.txt).

# Multi-ESP32 Piglet Wardriving Cluster

This document describes the coordinated master + headless scanner layout added on top of upstream Piglet/JCMK ESP-NOW mesh behavior.

## Roles

| Target | Sketch | Board (Arduino) | Role |
|--------|--------|-----------------|------|
| ESP32-S3 controller | `Arduino Files/Piglet/Piglet.ino` | **XIAO_ESP32S3** (recommended master) | GPS, SD, OLED, Web UI, mesh **Core**, optional WS2811 + buzzer |
| ESP32-C5 scanner | `PigletNode/PigletNode.ino` | **XIAO_ESP32C5** | Headless dual-band node (existing) |
| ESP32-C6 scanner | `PigletNodeC6/PigletNodeC6.ino` | **XIAO_ESP32C6** | Headless **2.4 GHz only** |
| ESP32-S3 scanner | `PigletNodeS3/PigletNodeS3.ino` | **XIAO_ESP32S3** | Headless **2.4 GHz only** |

Legacy Piglet builds on C5/C6 with full UI still work; the S3 master profile is the intended vehicle hardware.

## Flashing (Arduino CLI)

Install core 3.3.x:

```bash
arduino-cli core install esp32:esp32@3.3.10
arduino-cli lib install "TinyGPSPlus@1.0.3" "Adafruit GFX Library@1.12.6" "Adafruit BusIO@1.17.4" "ArduinoJson@7.4.3" "Adafruit SSD1306@2.5.17" "NimBLE-Arduino@2.5.1" "Adafruit NeoPixel@1.15.1"
```

Common FQBN flags: S3 master uses `PartitionScheme=default_8MB,PSRAM=opi`; XIAO ESP32-C6 headless uses `PartitionScheme=default` (4 MB).

```bash
FQBN_S3="esp32:esp32:XIAO_ESP32S3:CDCOnBoot=cdc,PartitionScheme=default_8MB,PSRAM=opi"
FQBN_C5="esp32:esp32:XIAO_ESP32C5:CDCOnBoot=cdc,PartitionScheme=default_8MB,PSRAM=enabled"
FQBN_C6="esp32:esp32:XIAO_ESP32C6:CDCOnBoot=cdc,PartitionScheme=default"

arduino-cli compile --fqbn "$FQBN_S3" "Arduino Files/Piglet"
arduino-cli compile --fqbn "$FQBN_C6" PigletNodeC6
arduino-cli compile --fqbn "$FQBN_S3" PigletNodeS3
arduino-cli compile --fqbn "$FQBN_C5" PigletNode
```

## Master wiring (ESP32-S3)

Authoritative GPIO map: `Arduino Files/Piglet/PinMapDefs.h` with `board=s3` in `/wardriver.cfg` (default XIAO ESP32-S3, not expansion base).

| Function | XIAO label | GPIO | Notes |
|----------|------------|------|--------|
| WS2811 data | **D0** | **1** | 330 Ω series; **verify** 5 V vs 12 V module and level shifting |
| Buzzer | **D2** | **3** | Active or passive via `buzzerType`; default config has `buzzerEnable=false` (mute) |
| User button | **D1** | **2** | `pins.btn` — unchanged |
| OLED I²C | — | 5 / 6 | Existing Piglet |
| SD SPI | — | 4 / 7 / 8 / 9 | Existing (`board=s3`, not `exp`) |
| GPS UART | — | 44 / 43 | Existing |

Use **`board=s3`** on the control node. With **`board=exp`**, GPIO3 is SD chip select and must not drive the buzzer.

### Pin conflict table (S3 control node)

| Silkscreen | GPIO | Piglet use | WS2811 | Buzzer |
|------------|------|------------|--------|--------|
| D0 | 1 | (feedback) | Data | — |
| D1 | 2 | Button | — | — |
| D2 | 3 | Buzzer (config) | — | Signal |
| D3+ | 4–9 | SD SPI | — | — |

## Configuration example (`/wardriver.cfg`)

```ini
board=s3
deviceRole=master
meshModeOnBoot=core

ws2811Enable=true
ws2811Pin=1
ws2811Brightness=64

buzzerEnable=false
buzzerPin=-1
buzzerType=active
alertGpsFix=true
alertNodeDisconnect=true
```

Headless nodes: no SD config required; board type is fixed in firmware.

## Mesh behavior

- JCMK ESP-NOW pairing, heartbeats, ADMIN channel assignments, and TEXT record forwarding are unchanged.
- **Capability-aware assignment:** C6 and S3 headless nodes only receive JCMK index range **0–13** (2.4 GHz). Dual-band C5 nodes receive slices across the full 40-channel table.
- Core OLED page 5 shows cluster totals: nodes, C6/S3/C5 counts, local net count, mesh records received, scan state, GPS.
- **OTA:** Separate SD images per board type (`/Firmware/PigletNodeC6.ota.bin`, `PigletNodeS3.ota.bin`, …). Images are validated by ESP chip ID before load/push — never flash C6 firmware onto S3 or vice versa. `Piglet.ota.bin` remains C5-only for ambiguous `PIGLET_BOARD_XIAO` pushes.

## LED / buzzer (master only)

Non-blocking states (priority high → low): critical error → node disconnect (purple) → GPS lost flash → scanning pulse → GPS fix → GPS search → boot white pulse.

Buzzer defaults **muted** (`buzzerEnable=false`). GPS alerts use ~4 s hysteresis to ignore brief dropouts.

## Test plan

| Area | Compile-only | Hardware |
|------|--------------|----------|
| S3 master build | `arduino-cli compile` | — |
| C6 / S3 node builds | compile | — |
| GPIO conflicts | review `PinMapDefs.h` + cfg | verify wiring |
| GPS → LED/buzzer | — | fix acquire/loss |
| C6 never on ch 36+ | core assigns + node clamp | scan log |
| Mixed C5+C6+S3 | — | Core page counts + records |
| Node timeout | — | power off node → purple + 3 beeps |
| Reconnect | — | power node back → re-pair |
| OTA chip check | load wrong `.ota.bin` on SD | Core should reject |

## Limitations

- One contiguous JCMK index range per node: dual-band nodes do not also take a separate 2.4-only partition algorithm beyond the dual-pool split.
- `PIGLET_BOARD_XIAO` OTA still targets C5 images only until board-specific Piglet master binaries are published per silicon.
- Physical WS2811/buzzer behavior was not validated in this environment (compile-only verification).

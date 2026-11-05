# S3 LED / buzzer test

Minimal sketch to verify **D0 (GPIO1)** WS2811/NeoPixel data and **D2 (GPIO3)** buzzer wiring on the Seeed XIAO ESP32-S3 — same pins as the Piglet control-node firmware.

## Arduino IDE

1. Open `S3_LED_Buzzer_Test.ino`
2. Board: **XIAO_ESP32S3**
3. Install **Adafruit NeoPixel** from Library Manager
4. Upload, open Serial Monitor **115200 baud**

## Expected behavior

- Boot: **white** LED ~400 ms, **one beep**, then Serial lines
- Loop: **red → green → blue → off** (~3 s cycle)

If Serial works but LED stays dark: check LED **power**, **GND**, data on **D0**, and that the module is **addressable** (one data wire).

Set `TEST_BUZZER` to `false` in the `.ino` if you only want to test the LED.

## Arduino CLI

```bash
arduino-cli lib install "Adafruit NeoPixel@1.15.1"
FQBN='esp32:esp32:XIAO_ESP32S3:CDCOnBoot=cdc'
arduino-cli compile --fqbn "$FQBN" examples/S3_LED_Buzzer_Test
arduino-cli upload  --fqbn "$FQBN" -p /dev/ttyACM0 examples/S3_LED_Buzzer_Test
```

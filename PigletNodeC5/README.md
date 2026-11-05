# PigletNodeC5 — Dual-band mesh scanner

Headless firmware for the **Seeed XIAO ESP32-C5**. No display, GPS, or SD card. It pairs with a Piglet Core and scans the channels the Core assigns, including 5 GHz.

The older `PigletNode/` sketch is the upstream C5 node. Use this folder for the cluster build (`v3.02-cluster`).

## Flash

Board: **XIAO_ESP32C5**

- USB CDC On Boot: **Enabled**
- Partition Scheme: **Default 8MB** (or any scheme with an app slot of at least 1 MB)
- PSRAM: **Enabled**

Open `PigletNodeC5.ino` and upload. Serial monitor is **115200**.

```
=== PigletNodeC5 Boot ===
JCMK channels: 40  |  home ch: 6
```

A healthy link looks like `ch 1-177` or a slice of that table, with `found` equal to `sent`. This board is supposed to tune 5 GHz. `unsupported channel` means the wrong sketch was flashed (C6 or S3).

## Controls

| Control | Action |
|---------|--------|
| BOOT button (GPIO0), hold > 2 s | Drop the Core and search again |
| LED fast blink | Searching |
| LED slow blink | Paired and scanning |

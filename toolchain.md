# Toolchain

Versions used to build and flash this project. Other versions may work; these are what's known-good. Pick **one** of the two toolchains below — both build the same source.

## Option A — Arduino IDE

| Tool | Version |
|------|---------|
| Arduino IDE | 2.3.7 |

### Board Package

| Package | Version |
|---------|---------|
| esp32 by Espressif Systems | 3.3.8 |

### Libraries (Arduino Library Manager)

| Library | Version | Author |
|---------|---------|--------|
| ArduinoJson | 7.4.3 | Benoit Blanchon |
| Adafruit NeoPixel | 1.15.4 | Adafruit |
| GFX Library for Arduino | 1.6.5 | moononournation |
| QRCode | 0.0.1 | ricmoo |

## Option B — PlatformIO

All versions are pinned in [platformio.ini](platformio.ini); `pio run` resolves them automatically.

| Component | Version | Notes |
|-----------|---------|-------|
| PlatformIO Core | 6.x | Install via VS Code PlatformIO IDE extension or `pip install platformio`. |
| platform-espressif32 (pioarduino fork) | 55.03.38-1 | Equivalent to Arduino-ESP32 core 3.3.8. The default upstream `espressif32` platform is on core 2.x and won't build (missing `esp32-hal-periman.h`). |
| ArduinoJson | ^7.4.3 | |
| Adafruit NeoPixel | ^1.15.4 | |
| GFX Library for Arduino | ^1.6.5 | |
| QRCode | ^0.0.1 | ricmoo |

## Vendored Library (both toolchains)

| Library | Version | Notes |
|---------|---------|-------|
| ArduinoWebsockets | 0.5.3 | Patched copy in [voice_agent_5/src/](voice_agent_5/src/) — do not install via Library Manager or add to `lib_deps`. See [websockets_patches.md](voice_agent_5/docs/websockets_patches.md). |
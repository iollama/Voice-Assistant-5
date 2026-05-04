# Voice Assistant 5 -- ESP32-S3 Voice Assistant

Meet your new desk companion: press the button, ask it anything, and get an instant spoken reply -- with an animated emoji face that grins, frowns, or laughs along with the conversation. It's small, expressive, and entirely yours -- a real voice assistant you built yourself, in a case you 3D-printed, powered by your own OpenAI account.

![Voice Assistant 5](images/va5_lead_image.jpg)

## Features

- **Press to talk, get an instant reply** -- hold the button to speak, release and it answers right back (no wake word, no waiting)
- **Animated emoji face shows you how the assistant feels** -- the assistant itself picks the mood and the display reacts in real time
- **Control the assistant personality** -- write who it is and how it behaves, right from your phone, no code needed
- **Glance to know what it's doing** -- a colored light tells you when it's listening, thinking, or speaking
- **Plug in your own OpenAI key** -- you control the account and the cost, no subscription in the middle
- **It remembers the conversation** -- pick up where you left off instead of starting from scratch
- **Set up WiFi once** -- connect from your phone the first time, it remembers your network forever
- **Build it yourself for ~$20** -- 3D-printed case included, full source open and hackable, parts links provided

## Initial Setup

1. **Connect to the device's WiFi.** On first boot the assistant creates an open WiFi network named `VOICE-AGENT-XXYY` (the `XXYY` is the last 2 bytes of its MAC address). No password.
2. **Set your home WiFi.** Your phone should pop the captive portal automatically -- if not, open a browser and go to `http://192.168.4.1`. Enter your WiFi name and password and hit **Save & Restart**. The device remembers it from then on.
3. **Open the settings page.** Once it's on your network, browse to `http://voice-agent-XXYY.local` (same `XXYY` as the AP name). If `.local` doesn't resolve on your network, use the IP address printed to the Serial Monitor.
4. **Make it your own.** By default the assistant is obsessed with Pokémon and will work them into every reply -- charming for about ten minutes. Edit the **System Prompt** field on the settings page to give it any persona you like.

## Under the Hood

A push-to-talk voice assistant built on the ESP32-S3, using the OpenAI Realtime API for speech-to-speech conversation over secure WebSockets. Features dual-core FreeRTOS architecture, animated emoji display with audio waveform visualization, a web configuration portal, and RGB LED status indicators.

Core 0 handles all network and protocol tasks (WiFi, WebSocket, Base64 encode/decode). Core 1 handles all hardware I/O (I2S microphone/speaker, button, display). The two cores communicate through PSRAM ring buffers and volatile flags.

## Hardware Components

| Component | Description | Comments | Link |
|-----------|-------------|----------|------|
| ESP32-S3 N16R8 | Dev board with 16 MB flash and 8 MB OPI PSRAM |buy the N16R8 varient | [Ali](https://s.click.aliexpress.com/e/_c3gDaUQx), [Amazon](https://amzn.to/4epGav2) |
| INMP441 | I2S MEMS microphone | - | [Ali](https://s.click.aliexpress.com/e/_c4aRLzSX), [Amazon](https://amzn.to/4tT7aI8) |
| MAX98357A | I2S audio amplifier breakout | Better to buy soldered| [Ali](https://s.click.aliexpress.com/e/_c3a1cRKJ), [Amazon](https://amzn.to/4w4aq4O) |
| Speaker | 1-3W 4/8-ohm or similar small speaker | Up to 57mm, use foam for pressure if the speaker is too thin | [Ali](https://s.click.aliexpress.com/e/_c3a1cRKJ), [Amazon](https://amzn.to/49pMOhj) |
| GC9A01 | Round TFT display, 240x240, SPI | Get the square one | [Ali](https://s.click.aliexpress.com/e/_c3MHXSVp), [Amazon](https://amzn.to/3RhVD6A) |
| WS2812 NeoPixel | RGB LED (built-in on most ESP32-S3 dev boards) | Nothing to buy, it's built in | - |
| Tactile Push Button | 12x12 mm momentary switch (PTT) | - | [Ali](https://s.click.aliexpress.com/e/_c3t0W2xV), [Amazon](https://amzn.to/4unYv0b) |
| Jumper Wires | Male-to-male/female (for the non PCB version) | - | [Ali](https://s.click.aliexpress.com/e/_c32s7dYx), [Amazon](https://amzn.to/48EsDvR) |
| USB-C Cable | Data + power, Type C to Type A | You should have one at home | [Ali](https://s.click.aliexpress.com/e/_c4mijwjN), [Amazon](https://amzn.to/3QNDeyr) |
| Breadboard | 400 Tie Points |You'll only need one power rail | [Ali](https://s.click.aliexpress.com/e/_c3MZmHuT), [Amazon](https://amzn.to/4w8xcZr) |
| Screws | M2 and M3 Self-Tapping Screws | I recommend a full kit | [Ali](https://s.click.aliexpress.com/e/_c3LsKZUF), [Amazon](https://s.click.aliexpress.com/e/_c3LsKZUF) |

## Pin Assignments

### Microphone -- INMP441

| INMP441 Pin | ESP32-S3 Pin | Note |
|-------------|-------------|------|
| SD (data) | GPIO 40 | I2S data in |
| WS | GPIO 41 | Word select / LRCLK |
| SCK | GPIO 42 | Bit clock |
| L/R | GND | Selects left channel |
| VDD | 3.3V | |
| GND | GND | |

### Speaker Amplifier -- MAX98357A

| MAX98357A Pin | ESP32-S3 Pin | Note |
|--------------|-------------|------|
| DIN | GPIO 17 | I2S data out |
| BCLK | GPIO 47 | Bit clock |
| LRC | GPIO 21 | Word select / LRCLK |
| SD | 3.3V | Amp enabled, left channel |
| VIN | 5V | |
| GAIN | Not connected | Default 9 dB gain |
| GND | GND | |

### Display -- GC9A01

| GC9A01 Pin | ESP32-S3 Pin | Note |
|------------|-------------|------|
| SCLK | GPIO 6 | SPI clock |
| MOSI (SDA) | GPIO 7 | SPI data |
| CS | GPIO 5 | Chip select |
| DC | GPIO 4 | Data/command |
| RST | GPIO 2 | Hardware reset |
| BLK | 3.3V | Backlight (always on) |
| VCC | 3.3V | |
| GND | GND | |

### Other

| Component | Pin | ESP32-S3 Pin | Note |
|-----------|-----|-------------|------|
| PTT Button | Signal | GPIO 1 | INPUT_PULLUP, active LOW |
| PTT Button | GND | GND | |
| NeoPixel RGB LED | Data | GPIO 48 | Built-in on most S3 boards, no jumper needed |

### Power Summary

| Rail | Components |
|------|-----------|
| 3.3V | INMP441, GC9A01, MAX98357A SD pin, GC9A01 BLK |
| 5V | MAX98357A VIN |
| GND | All components (common ground) |

## Arduino IDE Compilation Settings

Set these under the **Tools** menu before uploading:

| Setting | Value |
|---------|-------|
| **Board** | ESP32S3 Dev Module |
| **USB CDC On Boot** | Enabled |
| **CPU Frequency** | 240MHz (WiFi) |
| **Core Debug Level** | None |
| **USB DFU On Boot** | Disabled |
| **Erase All Flash Before Sketch Upload** | Disabled |
| **Events Run On** | Core 1 |
| **Flash Mode** | QIO 80MHz |
| **Flash Size** | 16MB (128Mb) |
| **JTAG Adapter** | Disabled |
| **Arduino Runs On** | Core 1 |
| **USB Firmware MSC On Boot** | Disabled |
| **Partition Scheme** | Custom (uses `partitions.csv` from sketch folder) |
| **PSRAM** | OPI PSRAM |
| **Upload Mode** | UART0 / Hardware CDC |
| **Upload Speed** | 921600 |
| **USB Mode** | Hardware CDC and JTAG |

The custom partition table (`partitions.csv`) provides 3 MB for the app and ~11 MB for LittleFS (emoji frames).

## Libraries

Install via Arduino Library Manager (`Sketch > Include Library > Manage Libraries...`):

| Library | Author | Note |
|---------|--------|------|
| **ArduinoJson** (v6.x+) | Benoit Blanchon | JSON parsing for API messages |
| **Adafruit NeoPixel** | Adafruit | RGB status LED driver |
| **Arduino_GFX_Library** | moononournation | Display driver (required only when `USE_EMOJIS 1`) |

**ArduinoWebsockets** is **vendored** in the `src/` directory with two critical bug fixes applied. Do **not** install it via Library Manager -- the sketch includes the local patched copy automatically. See [websockets_patches.md](voice_agent_5/docs/websockets_patches.md) for details.

## Configuration

### config.h

A `config.h.sample` file is included in the sketch folder. Copy it to `config.h` and fill in your credentials:

```bash
cp voice_agent_5/config.h.sample voice_agent_5/config.h
```

Then edit `config.h` and replace the placeholder values with your OpenAI API key (and optionally WiFi credentials):

```cpp
#define OPENAI_API_KEY "sk-your-openai-api-key"
#define WIFI_SSID "your-network-name"
#define WIFI_PASSWORD "your-network-password"
```

`config.h` is listed in `.gitignore` and will not be committed. WiFi credentials are also configurable at runtime via the captive portal and stored in NVS. The API key can also be set via the web UI (stored in NVS, overrides `config.h`).

### WiFi Setup (Captive Portal)

1. On first boot (or when saved credentials fail), the device creates a WiFi access point named `VOICE-AGENT-XXYY` (last 2 bytes of MAC)
2. The RGB LED flashes yellow to indicate configuration mode
3. Connect to this network with your phone or computer
4. A captive portal page opens automatically (or navigate to `192.168.4.1`)
5. Enter your WiFi SSID and password, then click **Save & Restart**
6. The device restarts and connects to your WiFi network
7. The RGB LED turns green when ready

### Agent Settings (Web Portal)

Once connected to WiFi, open a browser and navigate to `http://voice-agent-XXYY.local` (or the IP address printed to Serial Monitor).

From the settings page you can configure:

- **System Prompt** -- Control the assistant's persona and response style
- **Temperature** (0.0 - 2.0) -- Higher values produce more creative responses
- **Persist Conversation** -- When enabled, the assistant remembers previous exchanges via `previous_response_id` chaining
- **Verbose Logging** -- Enable detailed debug output on Serial Monitor
- **Volume** (0 - 100%) -- Speaker playback volume
- **API Key** -- Override the compiled `config.h` key (stored in NVS)

All settings are saved to NVS and persist across reboots. Use **Restore Defaults** to reset.

### Web Server Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Settings page |
| POST | `/save` | Save WiFi credentials and restart |
| POST | `/delete` | Forget WiFi credentials and restart into AP mode |
| POST | `/saveAgent` | Save agent configuration |
| POST | `/saveApiKey` | Save custom API key |
| POST | `/clearApiKey` | Revert to compiled API key |
| POST | `/saveVolume` | Save volume setting |
| POST | `/restoreAgent` | Restore default agent settings |

## Emoji Display Setup

### Prerequisites

- GC9A01 display wired per the pin table above
- `USE_EMOJIS` set to `1` in `voice_agent_5.ino` (default)
- Arduino_GFX_Library installed via Library Manager

### LittleFS Upload Plugin

The emoji animations are stored as pre-rendered RGB565 binary frames in a LittleFS partition. They must be flashed separately from the sketch.

**Install the plugin:**

1. Go to https://github.com/earlephilhower/arduino-littlefs-upload/releases
2. Download the latest `.vsix` file
3. Copy it to `~/.arduinoIDE/plugins/` (create the `plugins` folder if it doesn't exist)
4. Restart Arduino IDE

**Upload the filesystem:**

1. Close the Serial Monitor (it locks the COM port)
2. Press **Ctrl+Shift+P** to open the Command Palette
3. Type **Upload LittleFS** and select **"Upload LittleFS to Pico/ESP8266/ESP32"**
4. Wait for it to finish

The plugin packages the `data/` folder (containing 56 `.bin` frame files) into a LittleFS image and flashes it to the data partition. The sketch and filesystem go to different partitions -- they don't overwrite each other. You only need to re-upload the filesystem when the frame files change.

**Verify:** Open Serial Monitor after uploading both sketch and filesystem. You should see:

```
LittleFS mounted.
Display: loaded 8 frames for 'neutral'
Display: GC9A01 initialized
```

### Re-generating Emoji Frames (Optional)

If you want to regenerate the `data/*.bin` files from the source GIFs in `feelings/`:

**Requirements:** Python 3 with Pillow (`pip install Pillow`)

**Run:**

```bash
cd voice_agent_5
python utils/convert_gifs.py
```

This extracts 8 evenly-spaced frames from each GIF, resizes to 150x150, converts to RGB565 little-endian binary, and writes to `data/<emotion>_<frame>.bin`.

### Emoji Credits

Emoji animations sourced from [Google Fonts Emoji](https://googlefonts.github.io/), used under the [Creative Commons Attribution 4.0 International License (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/legalcode).

## Library Patches

ArduinoWebsockets v0.5.3 has two upstream bugs that prevent WSS connections on ESP32: a missing `setInsecure()` method in `SecuredEsp32TcpClient`, and a missing fallback to insecure mode in the ESP32 branch of `upgradeToSecuredConnection()`. Both cause every secure WebSocket connection to fail silently.

The library is vendored in `src/` with fixes already applied -- no manual patching required. For full details, the exact code changes, and instructions for updating to a newer version, see [websockets_patches.md](voice_agent_5/docs/websockets_patches.md).

## Tuning Knobs

### Compile-Time Constants

These are defined in `voice_agent_5.ino` and require recompilation to change:

| Constant | Default | Description |
|----------|---------|-------------|
| `SAMPLE_RATE_IN` | 16000 | Microphone sample rate (Hz). Must match INMP441 config. |
| `SAMPLE_RATE_OUT` | 24000 | Speaker playback rate (Hz). Must match OpenAI Realtime API output format. |
| `JITTER_BUFFER_MS` | 300 | Milliseconds of audio to buffer before starting playback. Higher = smoother but more latency. |
| `LOW_WATER_BYTES` | 3072 | Rebuffer threshold (~64 ms at 24 kHz). When the speaker buffer drops below this during playback, pause and re-accumulate before resuming. |
| `DMA_BLOCK_SIZE` | 1024 | I2S DMA transfer size in bytes per read/write call. |
| `WIFI_CONNECT_TIMEOUT_MS` | 15000 | Time to wait for WiFi before falling back to AP config mode. |
| `EMOJI_FRAME_MS` | 120 | Delay between emoji animation frames (~8 fps). Lower = faster animation. |
| `EMOJI_SIZE_PX` | 150 | Emoji frame dimensions (width and height). Must match `convert_gifs.py` output. |
| `EMOJI_NUM_FRAMES` | 8 | Number of animation frames per emotion. Must match `convert_gifs.py` output. |
| `USE_EMOJIS` | 1 | Set to `0` to disable display entirely (serial-only mode, no GC9A01 or Arduino_GFX needed). |

### Runtime Settings (Web UI / NVS)

These are configurable at runtime via the web portal and persist across reboots:

| Setting | Default | Range | Description |
|---------|---------|-------|-------------|
| System Prompt | "You are a helpful voice assistant..." | up to 2000 chars | Controls assistant persona |
| Temperature | 0.7 | 0.0 - 2.0 | Model creativity |
| Persist Conversation | true | boolean | Chain responses via `previous_response_id` |
| Verbose Logging | true | boolean | Detailed serial debug output |
| Volume | 100 | 0 - 100 | Speaker volume (PCM sample scaling) |

## 3D Printed Enclosure

The `box/` directory contains files for a circular 145 mm enclosure designed to hold all components:

- **`box/box3.scad`** -- Parametric OpenSCAD source. Customizable dimensions, component positions, and clearances. Set `part_to_print` to `"tray"`, `"ring"`, `"lid"`, or `"all"`.
- **`box/box3.stl`** -- Pre-generated STL mesh, ready to slice and print.

The design uses a tray/ring/lid construction: the tray holds all components (ESP32, display, speaker, mic, amplifier, button) facing down, the ring forms the enclosure wall with USB cutout, and the lid closes the back. Parts are secured with M3 screws into printed bosses.

## State Machine / LED Colors

| State | LED Color | Meaning |
|-------|-----------|---------|
| `STATE_WIFI_WAIT` | Yellow (solid) | Connecting to WiFi |
| `STATE_WIFI_CONFIG` | Yellow (flashing) | AP mode, awaiting WiFi credentials |
| `STATE_READY_FOR_INPUT` | Green | Idle, ready for push-to-talk |
| `STATE_RECORDING` | Pink | Mic active, streaming audio to OpenAI |
| `STATE_THINKING` | Blue | Waiting for API response |
| `STATE_SPEAKING` | Cyan | Playing response audio |

## Architecture Documentation

For detailed technical documentation covering dual-core architecture, ring buffer internals, audio pipeline, protocol details, emoji display system, and memory layout, see [architecture.md](voice_agent_5/docs/architecture.md).

## Troubleshooting

- **LED stays yellow**: WiFi connection failed. The device will fall back to SoftAP mode after 15 seconds. Check your WiFi credentials.
- **No sound from speaker**: Verify the MAX98357A SD pin is connected to 3.3V (not GND).
- **Microphone not picking up audio**: Check INMP441 wiring (especially L/R to GND for left channel).
- **Can't reach `.local` address**: mDNS may not work on all networks/devices. Use the IP address printed to Serial Monitor instead.
- **Display shows text instead of emoji**: LittleFS filesystem not uploaded, or frame files missing. Re-upload the filesystem using the LittleFS plugin.
- **"Display: gfx->begin() FAILED!"**: Check display SPI wiring (especially DC, CS, RST pins).

## License

This project is licensed under [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) (Creative Commons Attribution-NonCommercial 4.0 International).

You are free to use, modify, and share this project for **non-commercial purposes**, provided you give appropriate attribution.

**For commercial use**, please contact the creator at hello@oriliventures.com to arrange a license.

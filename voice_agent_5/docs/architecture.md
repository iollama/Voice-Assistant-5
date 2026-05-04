# Voice Agent 5 — Architecture Document

ESP32-S3 push-to-talk voice assistant using the OpenAI Realtime API (speech-to-speech over secure WebSockets), with emotion tool calling for display state feedback.

---

## System Overview

```
  User presses PTT button
         │
         ▼
  [I2S Mic DMA] ──► [Input Ring Buffer] ──► [WebSocket] ──► OpenAI Realtime API
                                                                      │
  [I2S Speaker DMA] ◄── [Output Ring Buffer] ◄── [WebSocket] ◄───────┘
                                                       │
                                              set_display_emotion()
                                              tool call (JSON) parsed
                                              → printed to Serial
```

Audio captured from microphone is streamed in real-time to OpenAI. The API responds with synthesized speech audio, which is buffered and played through the speaker. The entire pipeline is speech-to-speech with no local STT or TTS processing. Each response is preceded by an emotion tool call that signals the emotional tone of the reply.

---

## Source File Organization

The sketch is split across multiple `.ino` files in the sketch folder. Arduino IDE concatenates them alphabetically after the main file before compilation, so all globals are shared without `extern` declarations. `ring_buffer.h` is a header included explicitly.

| File | Contents |
|------|----------|
| `voice_agent_5.ino` | Includes, all globals/defines/enums, `setup()`, `loop()` |
| `ring_buffer.h` | `PSRAMRingBuffer` class |
| `display.ino` | `emoji_init_display()`, `emoji_show_emotion()`, `updateDisplay()` |
| `hardware.ino` | I2S setup, `writeSilence()`, `updateButtonState()`, `captureMicrophone()`, `manageSpeaker()` |
| `protocol.ino` | `handleAudioDelta()`, `handleEmotionToolCall()`, `handleResponseDone()`, `manage_websockets()`, `protocol_task()`, `sendAudioChunk()`, `jsonEscape()`, `get_api_key()` |
| `webserver.ino` | `load_agent_config()`, `setup_web_server()`, all `handle_*` HTTP handlers |
| `wifi.ino` | `connectToWiFi()`, `setupMDNS()`, `syncTime()`, `init_wifi_and_time()`, `start_soft_ap()` |

The `src/` folder contains the vendored ArduinoWebsockets library and is not part of the sketch code.

---

## Dual-Core FreeRTOS Architecture

The firmware splits responsibilities across both ESP32-S3 cores to keep audio hardware and network I/O from interfering with each other.

| Core | Task | Responsibilities |
|------|------|-----------------|
| Core 0 | `protocol_task` | WiFi, WSS connection, JSON parsing, Base64 encode/decode, sending/receiving audio data, emotion tool call handling |
| Core 1 | `loop()` (Arduino main) | Delegates to `updateButtonState()`, `captureMicrophone()`, `updateDisplay()`, `manageSpeaker()` — defined in `hardware.ino` and `display.ino` |

The two cores communicate exclusively through shared primitives:
- `PSRAMRingBuffer* in_ring_buf` — mic audio from Core 1 → Core 0
- `PSRAMRingBuffer* out_ring_buf` — speaker audio from Core 0 → Core 1
- `volatile bool` flags: `cmd_cancel`, `cmd_commit`, `ws_response_done`

Emotion tool call state is managed entirely within Core 0 (protocol task only):
- `g_awaiting_tool_response` — set when a tool call is in flight, cleared on `response.done`
- `g_tool_result_ready` — signals the main protocol loop to submit the tool result and re-request audio
- `g_pending_call_id` — stores the `call_id` from the tool call event, required for the function result submission

---

## Ring Buffer (`PSRAMRingBuffer`)

The two cores share audio data through `PSRAMRingBuffer` — a byte-level circular buffer defined in `ring_buffer.h`. Each instance is allocated in PSRAM via `ps_malloc()` and protected by a FreeRTOS mutex.

### API

| Method | Signature | Behavior |
|--------|-----------|----------|
| `write` | `bool write(const uint8_t* data, size_t len)` | Returns `false` if insufficient space (non-blocking overflow protection). Copies bytes into the buffer, advancing `head`. |
| `read` | `size_t read(uint8_t* data, size_t len)` | Returns actual bytes read (may be less than requested). Copies bytes out, advancing `tail`. |
| `available` | `size_t available()` | Returns number of bytes currently buffered. |
| `clear` | `void clear()` | Resets head, tail, and count to zero. Used for barge-in. |
| `isValid` | `bool isValid() const` | Returns `true` if both `ps_malloc()` and `xSemaphoreCreateMutex()` succeeded. |

All methods except `isValid` acquire the mutex with `portMAX_DELAY` (blocking). Critical sections are short byte-copy loops, so contention between cores is minimal.

### Instances

| Buffer | Capacity | Writer (Core) | Reader (Core) | Purpose |
|--------|----------|---------------|---------------|---------|
| `in_ring_buf` | 64 KB (2s @ 16kHz 16-bit) | Core 1 — `captureMicrophone()` | Core 0 — `sendAudioChunk()` | Mic PCM16 staging before Base64 encode and WebSocket send |
| `out_ring_buf` | 1,440 KB (30s @ 24kHz 16-bit) | Core 0 — `handleAudioDelta()` | Core 1 — `manageSpeaker()` | Decoded speaker PCM16 staging before I2S DMA write |

### Why PSRAM

The combined buffer sizes (64 KB + 1,440 KB + 48 KB decode buffer) far exceed available internal SRAM. `ps_malloc()` places them in the 8 MB OPI PSRAM, keeping the internal heap free for the WiFi/TLS stack and ArduinoWebsockets library allocations.

### Why Mutex

Both cores access each buffer concurrently — Core 1 writes mic data that Core 0 reads, and Core 0 writes speaker data that Core 1 reads. The FreeRTOS mutex ensures atomic updates to `head`, `tail`, and `count`. Without it, a context switch mid-update could corrupt the ring buffer state.

### Barge-In

When PTT is pressed during playback, `out_ring_buf->clear()` discards all buffered speaker audio instantly. Combined with `response.cancel` sent over WebSocket, this enables immediate mic capture without waiting for old audio to drain.

---

## State Machine

The assistant has a single global state `g_state` (declared `volatile`) that both cores read. Only designated code paths write it.

```
                    ┌─────────────────────────────┐
                    │                             │
              WiFi fails                    WS disconnects
                    │                             │
  Boot ──► STATE_WIFI_WAIT ──► STATE_WIFI_CONFIG  │
                    │                             │
              WiFi + WS OK                        │
                    │                             │
                    ▼                             │
          STATE_READY_FOR_INPUT ◄─────────────────┘
            (LED: green)
                    │
            PTT pressed
                    │
                    ▼
          STATE_RECORDING
            (LED: pink)
                    │
            PTT released
                    │
                    ▼
          STATE_THINKING
            (LED: blue)
                    │
         Jitter threshold met
         OR response.done + buffer > 0
                    │
                    ▼
          STATE_SPEAKING
            (LED: cyan)
                    │
         Buffer empty + response.done
                    │
                    └──► STATE_READY_FOR_INPUT
```

### State → LED Color

| State | LED Color | Meaning |
|-------|-----------|---------|
| `STATE_WIFI_WAIT` | Yellow | Connecting to WiFi / TLS |
| `STATE_WIFI_CONFIG` | Flashing yellow | AP mode, awaiting WiFi credentials |
| `STATE_READY_FOR_INPUT` | Green | Idle, ready for PTT |
| `STATE_RECORDING` | Pink | Mic active, streaming to OpenAI |
| `STATE_THINKING` | Blue | Waiting for response |
| `STATE_SPEAKING` | Cyan | Playing response audio |

---

## Hardware

For complete pin assignments and wiring tables, see the [Pin Assignments](../../readme.md#pin-assignments) section in the README.

### I2S Configuration

- **Microphone (INMP441):** `I2S_NUM_0`, master RX, 16kHz, 32-bit samples (INMP441 output), left channel only. The 32-bit samples are right-shifted 16 bits in software to produce 16-bit PCM for transmission.
- **Speaker (MAX98357A):** `I2S_NUM_1`, master TX, 24kHz, 16-bit samples, left channel only. `tx_desc_auto_clear = true` prevents DMA underrun noise.

---

## Audio Pipeline Detail

### Recording (Core 1 → Core 0)

```
INMP441
  │ 32-bit I2S frames @ 16kHz
  ▼
i2s_read() [1024 byte chunks]
  │ Right-shift 16: int32 → int16 (PCM16)
  ▼
in_ring_buf (PSRAM, 64KB = 2s @ 16kHz 16-bit)
  │
  ▼ (Core 0 reads when STATE_RECORDING)
sendAudioChunk() — encodes 2048 bytes into s_b64_buf, builds JSON in s_ws_frame
  │ no heap allocation
  ▼
WebSocket: input_audio_buffer.append {audio: base64}
```

The mic DMA is always drained even when not recording to prevent stale audio accumulating in the DMA FIFO.

On PTT release, the remaining bytes in `in_ring_buf` are flushed in the same 2048-byte chunks via the same `sendAudioChunk()` path before sending `input_audio_buffer.commit`.

### Playback (Core 0 → Core 1)

```
WebSocket: response.audio.delta {delta: base64}
  │
  ▼ (Core 0 onMessage callback)
Base64 decode into s_decode_buf (PSRAM, 48KB pre-allocated) — no heap allocation
  │ raw PCM16 bytes
  ▼
out_ring_buf (PSRAM, 1,440KB = 30s @ 24kHz 16-bit)
  │
  ▼ (Core 1 loop, when STATE_THINKING or STATE_SPEAKING)
Jitter buffer gate: wait for 14,400 bytes (300ms)
  OR: if response.done and any bytes waiting → start immediately
  │
  ▼ (STATE_SPEAKING)
Volume scaling: sample[i] = sample[i] * g_volume / 100
  │
  ▼
i2s_write() [1024 byte chunks] → MAX98357A → speaker
```

**Jitter buffer:** The 300ms gate prevents choppy playback caused by network jitter — playback only begins once enough audio has buffered. For short responses that complete before the buffer fills, the `response.done` event bypasses the threshold and starts playback immediately with whatever is buffered.

**Barge-in:** When PTT is pressed while the assistant is speaking, `out_ring_buf` is cleared and `response.cancel` is sent to OpenAI, immediately stopping the current response. The microphone starts capturing the new input without delay.

---

## Protocol Task (Core 0)

`manage_websockets()` runs in a loop on Core 0 and handles the full WebSocket lifecycle.

### Connection Sequence

1. Wait for `WiFi.status() == WL_CONNECTED`
2. Connect via `wsClient.connectSecure("api.openai.com", 443, "/v1/realtime?model=gpt-4o-realtime-preview")`
3. Send `session.update` to configure the session:
   - Modalities: audio + text
   - Voice: alloy
   - Input/output format: pcm16
   - Turn detection: **null** (manual PTT, not VAD)
   - System instruction and temperature from NVS (also printed to Serial at this point)
   - Tool: `set_display_emotion` with emotion enum and intensity float
   - `tool_choice: "required"` — model must call the tool on every response

### WebSocket Message Handling

Incoming messages are dispatched from the `onMessage` lambda to three dedicated handlers defined in `protocol.ino`. Parsing is manual `String::indexOf()` on the hot path (`handleAudioDelta`); ArduinoJson is used only for the infrequent emotion tool call (`handleEmotionToolCall`) where latency is not critical.

| Event type | Action |
|-----------|--------|
| `response.audio.delta` | Extract `"delta"` field, Base64 decode, write to `out_ring_buf` |
| `response.function_call_arguments.done` (name: `set_display_emotion`) | Parse `emotion` + `intensity` via ArduinoJson, store `call_id`, set `g_awaiting_tool_response = true`, print mood to Serial |
| `response.done` / `response.audio.done` — tool pending | Clear `g_awaiting_tool_response`, set `g_tool_result_ready = true` |
| `response.done` / `response.audio.done` — no tool pending | Set `ws_response_done = true` |
| `error` | Print to serial |
| anything else | Print to serial if verbose and payload < 500 bytes |

### Emotion Tool Call Flow

The model calls `set_display_emotion` before generating audio. Because the Realtime API treats a tool call as a complete response, audio does not follow automatically — the client must submit a result and re-request.

```
response.function_call_arguments.done
  → store call_id, set g_awaiting_tool_response, print MOOD to Serial

response.done  (tool-only response ends)
  → g_awaiting_tool_response = false
  → g_tool_result_ready = true   (lambda cannot send — no wsClient access)

main loop picks up g_tool_result_ready
  → send conversation.item.create {type: function_call_output, call_id, output: "ok"}
  → send response.create
  → audio response arrives normally
```

The `g_tool_result_ready` flag is the bridge between the captureless `onMessage` lambda (which cannot call `wsClient.send()`) and the main protocol loop where `wsClient` is in scope.

### Outgoing Commands (polled every 5ms)

| Flag / condition | Action |
|-----------------|--------|
| `g_tool_result_ready = true` | Send `conversation.item.create` (function result) + `response.create`, clear flag |
| `cmd_cancel = true` | Send `response.cancel`, clear `ws_response_done` |
| `STATE_RECORDING` and `in_ring_buf >= 2048 bytes` | Send `input_audio_buffer.append` chunk |
| `cmd_commit = true` | Flush remaining mic buffer, send `input_audio_buffer.commit`, send `response.create` |

---

## Emotion Tool Definition

The `set_display_emotion` tool is registered in `session.update` and gives the model a structured side-channel to communicate emotional tone alongside the audio response.

```json
{
  "type": "function",
  "name": "set_display_emotion",
  "description": "Set the display animation state to match the emotional tone of your response. Call this at the start of each response.",
  "parameters": {
    "type": "object",
    "properties": {
      "emotion": {
        "type": "string",
        "enum": ["neutral","happy","excited","empathetic","confused","concerned","thinking"]
      },
      "intensity": { "type": "number", "minimum": 0.0, "maximum": 1.0 }
    },
    "required": ["emotion"]
  }
}
```

The model selects the enum value that best matches the emotional register of its response. The `description` field is the primary signal — it is natural language read by the model's reasoning layer. The enum constrains the vocabulary so the model cannot invent values outside the defined set.

When `USE_EMOJIS` is enabled, the emotion value drives animated emoji display on the GC9A01 TFT (see [Emoji Display System](#emoji-display-system) below). The emotion is also printed to Serial for debugging.

---

## Configuration & Web UI

Configuration is stored in ESP32 NVS (Non-Volatile Storage) using the `Preferences` library and is editable via a built-in HTTP server on port 80.

| Setting | NVS Key | Default |
|---------|---------|---------|
| System prompt | `sysPrompt` | "You are a helpful voice assistant..." |
| Temperature | `temp` | 0.7 |
| Persist conversation | `persist` | true |
| Verbose logging | `verbose` | true |
| API key | `apiKey` | falls back to `config.h` |
| Volume | `volume` | 100 |
| WiFi SSID | `wifi-creds/ssid` | — |
| WiFi password | `wifi-creds/pass` | — |

### WiFi Setup (Captive Portal)

On first boot (no credentials in NVS), or after a failed connection, the device starts a soft AP named `VOICE-AGENT-XXYY` (last 2 bytes of MAC). A DNS captive portal redirects all traffic to the config page at `192.168.4.1`. Once credentials are saved, the device restarts and connects in STA mode.

After successful connection, the web UI is accessible at `http://voice-agent-XXYY.local` (mDNS) or via IP.

---

## Emoji Display System

Animated emoji faces are rendered on a GC9A01 round 240x240 TFT display, driven by the `set_display_emotion` tool call from the OpenAI Realtime API. All display code is gated by the `USE_EMOJIS` compile flag (set to `0` for serial-only mode with no display hardware required).

### Display Hardware

| Component | Detail |
|-----------|--------|
| Display | GC9A01 round TFT, 240x240, SPI |
| Library | Arduino_GFX_Library by moononournation |

| Display Pin | ESP32-S3 GPIO |
|-------------|--------------|
| SCLK | 6 |
| MOSI (SDA) | 7 |
| CS | 5 |
| DC | 4 |
| RST | 2 |
| BLK (backlight) | 3.3V |

### Why Arduino_GFX Instead of TFT_eSPI

TFT_eSPI v2.5.43 crashed on ESP32-S3 with `StoreProhibited` during `tft.init()` — global constructor crash before `setup()` even ran, SPI bus crash regardless of port selection, and heap assertion from fragmented internal RAM. Arduino_GFX_Library worked immediately with zero issues on the same hardware and pins, with first-class ESP32-S3 and GC9A01 support.

### Display Layout

```
┌─────────────────────┐  <- round 240x240 display
|                     |
|   ┌───────────┐     |
|   |  150x150  |     |  <- emoji face, centered horizontally
|   |   emoji   |     |     ~10px top margin
|   └───────────┘     |
|                     |
|  ▁▂▄▆▄▂▁▂▄▆▄▂▁     |  <- waveform bar, ~40px tall
|                     |     ~10px bottom margin
└─────────────────────┘
```

- **Emoji zone:** 150x150px, centered horizontally (`EMOJI_X_OFFSET = 45`), 10px top margin
- **Waveform zone:** 16 bars across full width, ~36px tall, bottom of display

### Frame Storage (LittleFS)

The custom partition scheme (`partitions.csv`) allocates ~11 MB for a LittleFS data partition:

| Partition | Offset | Size |
|-----------|--------|------|
| app0 | 0x10000 | 3 MB |
| spiffs (LittleFS) | 0x310000 | ~11 MB |

Emoji frames are stored as raw RGB565 binary files:
- 7 emotions x 8 frames = 56 files
- File naming: `/<emotion>_<frame>.bin` (e.g., `/happy_0.bin`)
- Each file: 45,000 bytes (150 x 150 x 2 bytes RGB565, little-endian)
- Total: ~2.5 MB

### Runtime Flow

1. **Initialization** (`emoji_init_display()`): Creates SPI bus and GC9A01 driver via Arduino_GFX. Allocates 8 x 45 KB frame buffers in PSRAM.

2. **WiFi pulse:** During WiFi connection, a breathing blue circle animation is drawn (`drawWifiPulse()`). Cleared by `clearWifiPulse()` once connected.

3. **Emotion change:** When `g_current_emotion` (set by Core 0 protocol task) differs from `g_loaded_emotion`, `emoji_show_emotion()` loads all 8 frames for the new emotion from LittleFS into PSRAM (~50ms), then displays the first frame.

4. **Animation loop:** `updateDisplay()` is called every Core 1 loop iteration. It advances the emoji frame every `EMOJI_FRAME_MS` (120ms, ~8fps) and redraws the waveform bar on every iteration for smooth animation.

5. **Waveform bar:** 16 vertical bars with a sine envelope (taller in center). Color is green during `STATE_SPEAKING` (AI audio), blue during `STATE_RECORDING` (user mic). Amplitude tracks `g_audio_level` (0-255) with exponential smoothing (fast attack, slow decay). Per-bar sinusoidal wobble creates wave motion.

### Emoji Attribution

Emoji animations sourced from [Google Fonts Emoji](https://googlefonts.github.io/), used under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/legalcode).

---

## Memory Layout

| Buffer | Location | Size | Purpose |
|--------|----------|------|---------|
| `in_ring_buf` | PSRAM | 64 KB | Mic audio staging (2s @ 16kHz 16-bit) |
| `out_ring_buf` | PSRAM | 1,440 KB | Speaker audio staging (30s @ 24kHz 16-bit) |
| `s_decode_buf` | PSRAM | 48 KB | Audio delta decode output (pre-allocated, reused each delta) |
| `g_frame_bufs[8]` | PSRAM | 360 KB | Preloaded emoji frames for current emotion (8 x 45 KB) |
| `s_mic_chunk` | Internal SRAM (.bss) | 2 KB | Mic DMA read staging, encode input |
| `s_b64_buf` | Internal SRAM (.bss) | 2.7 KB | Base64 encode output (ceil(2048×4/3)+4) |
| `s_ws_frame` | Internal SRAM (.bss) | 2.8 KB | JSON frame for audio append |
| I2S TX DMA | Internal SRAM | ~8 KB | Speaker DMA (8 buffers × 256 samples × 2 bytes) |
| I2S RX DMA | Internal SRAM | ~8 KB | Mic DMA (8 buffers × 256 samples × 4 bytes) |
| WebSocket payload (String) | Heap | up to ~32 KB | Audio delta JSON — allocated by ArduinoWebsockets, freed after callback |

All audio processing buffers are pre-allocated at boot. The only remaining heap activity during normal operation is inside `wsClient.send()` and `wsClient.poll()` (ArduinoWebsockets internals), which are short-lived and fixed-pattern.

PSRAM buffers are allocated via `ps_malloc()` to keep the internal heap free for the TLS stack and WebSocket library.

---

## Library Patches

ArduinoWebsockets v0.5.3 is **vendored** into the sketch's `src/` directory with two critical bug fixes applied. See [websockets_patches.md](websockets_patches.md) for the exact code changes, motivation, and upgrade instructions.

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
                                              set_volume()
                                              tool calls (JSON) parsed
                                              → effects applied, results returned
```

Audio captured from microphone is streamed in real-time to OpenAI. The API responds with synthesized speech audio, which is buffered and played through the speaker. The entire pipeline is speech-to-speech with no local STT or TTS processing. Each response is preceded by an emotion tool call that signals the emotional tone of the reply.

---

## Source File Organization

The sketch is split across multiple `.ino` files in the sketch folder. The Arduino preprocessor (used by both Arduino IDE and PlatformIO with `framework = arduino`) concatenates them alphabetically after the main file before compilation, so all globals are shared without `extern` declarations. `ring_buffer.h` is a header included explicitly.

| File | Contents |
|------|----------|
| `voice_agent_5.ino` | Includes, all globals/defines/enums, `setup()`, `loop()` |
| `ring_buffer.h` | `PSRAMRingBuffer` class |
| `display.ino` | `emoji_init_display()`, `emoji_show_emotion()`, `updateDisplay()`, `display_boot_status()`, `display_info_screen()`, `display_captive_portal_screen()` |
| `hardware.ino` | I2S setup, `writeSilence()`, `updateButtonState()`, `captureMicrophone()`, `manageSpeaker()` |
| `protocol.ino` | `handleAudioDelta()`, `handleEmotionToolCall()`, `handleResponseDone()`, `manage_websockets()`, `protocol_task()`, `sendAudioChunk()`, `jsonEscape()`, `get_api_key()` |
| `webserver.ino` | `load_agent_config()`, `setup_web_server()`, all `handle_*` HTTP handlers (including the emoji web API) |
| `wifi.ino` | `connectToWiFi()`, `setupMDNS()`, `syncTime()`, `init_wifi_and_time()`, `start_soft_ap()` |
| `emojis_page.h` | Static HTML+JS for `GET /emojis` — included from `webserver.ino`. Lives in its own header because the Arduino auto-prototyper mis-parses `async function` inside a raw string as a C++ prototype. |

The `src/` folder contains the vendored ArduinoWebsockets library and is not part of the sketch code.

---

## Dual-Core FreeRTOS Architecture

The firmware splits responsibilities across both ESP32-S3 cores to keep audio hardware and network I/O from interfering with each other.

| Core | Task | Responsibilities |
|------|------|-----------------|
| Core 0 | `protocol_task` | WiFi, WSS connection, JSON parsing, Base64 encode/decode, sending/receiving audio data, emotion and volume tool call handling |
| Core 1 | `loop()` (Arduino main) | Delegates to `updateButtonState()`, `captureMicrophone()`, `updateDisplay()`, `manageSpeaker()` — defined in `hardware.ino` and `display.ino` |

The two cores communicate exclusively through shared primitives:
- `PSRAMRingBuffer* in_ring_buf` — mic audio from Core 1 → Core 0
- `PSRAMRingBuffer* out_ring_buf` — speaker audio from Core 0 → Core 1
- `volatile bool` flags: `cmd_cancel`, `cmd_commit`, `ws_response_done`

Volume tool call state crosses cores:
- `g_volume` — written by Core 0 with the new level; read by Core 1's `manageSpeaker()` for PCM scaling
- `g_volume_dirty` — set by Core 0, cleared by Core 1 after NVS write

Network info screen crosses cores:
- `g_show_info_screen` — set by Core 0 (`handleNetworkInfoToolCall`); read and cleared by Core 1 on PTT press. While true, `loop()` shows the info screen and suppresses normal emoji display.

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
| `in_ring_buf` | 96 KB (2s @ 24kHz 16-bit) | Core 1 — `captureMicrophone()` | Core 0 — `sendAudioChunk()` | Mic PCM16 staging before Base64 encode and WebSocket send |
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

- **Microphone (INMP441):** `I2S_NUM_0`, master RX, 24kHz, 32-bit samples (INMP441 output), left channel only. The 32-bit samples are right-shifted 16 bits in software to produce 16-bit PCM for transmission.
- **Speaker (MAX98357A):** `I2S_NUM_1`, master TX, 24kHz, 16-bit samples, left channel only. `tx_desc_auto_clear = true` prevents DMA underrun noise.

---

## Audio Pipeline Detail

### Recording (Core 1 → Core 0)

```
INMP441
  │ 32-bit I2S frames @ 24kHz
  ▼
i2s_read() [1024 byte chunks]
  │ Right-shift 16: int32 → int16 (PCM16)
  ▼
in_ring_buf (PSRAM, 96KB = 2s @ 24kHz 16-bit)
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
WebSocket: response.output_audio.delta {delta: base64}
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
2. Connect via `wsClient.connectSecure("api.openai.com", 443, "/v1/realtime?model=gpt-realtime-2")` — no `OpenAI-Beta` header on the GA API
3. Send `session.update` to configure the session (GA schema — fields are nested under `session.audio`):
   - `session.type`: `"realtime"` (required discriminator on GA)
   - `session.audio.output.voice`: read from `g_voice` (NVS-backed, defaults to `marin`; whitelist of the 10 SDK voices — `alloy`, `ash`, `ballad`, `coral`, `echo`, `sage`, `shimmer`, `verse`, `marin`, `cedar`)
   - `session.audio.input.format` / `session.audio.output.format`: `{type: "audio/pcm", rate: 24000}` — the SDK locks `rate` to exactly 24000
   - `session.audio.input.turn_detection`: **null** (manual PTT, not VAD)
   - `session.reasoning.effort`: `"low"` (hard-coded — OpenAI's recommended default for production voice agents)
   - `session.instructions` from NVS (printed to Serial at this point); the `temperature` field was removed in GA so it is not sent
   - Tools: `set_display_emotion` (emotion enum + intensity float), `set_volume` (absolute level 0–100 or relative delta -100..100), `show_network_info` (no params — displays IP QR + token count on screen, stays until next PTT press, returns `"ok"`). The schema for each tool is built automatically from `TOOLS[]` in `tools.ino` by `buildToolsJson()`.
   - `tool_choice: "auto"` — model calls tools when appropriate

### WebSocket Message Handling

Incoming messages are dispatched from the `onMessage` lambda in `protocol.ino`. Parsing is manual `String::indexOf()` on the hot path (`handleAudioDelta`); ArduinoJson is used only for infrequent tool calls where latency is not critical.

Tool calls are routed through `dispatchToolCall()` in `tools.ino`, which iterates the `TOOLS[]` registry and calls the matching handler by name.

| Event type | Action |
|-----------|--------|
| `response.output_audio.delta` | Extract `"delta"` field, Base64 decode, write to `out_ring_buf` |
| `response.function_call_arguments.done` (name: `set_display_emotion`) | Parse `emotion` + `intensity` via ArduinoJson, store `call_id`, set `g_awaiting_tool_response = true`, print mood to Serial |
| `response.function_call_arguments.done` (name: `set_volume`) | Parse `volume` or `delta` via ArduinoJson, clamp result to 0–100, write `g_volume`, set `g_volume_persist_pending = true`; store `call_id`, set `g_awaiting_tool_response = true` |
| `response.function_call_arguments.done` (name: `show_network_info`) | Store `call_id`, set `g_show_info_screen = true`, set `g_awaiting_tool_response = true` |
| `response.done` / `response.output_audio.done` — tool pending | Clear `g_awaiting_tool_response`, set `g_tool_result_ready = true` |
| `response.done` / `response.output_audio.done` — no tool pending | Set `ws_response_done = true` |
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

When `USE_DISPLAY` is enabled, the emotion value drives animated emoji display on the GC9A01 TFT (see [Emoji Display System](#emoji-display-system) below). The emotion is also printed to Serial for debugging.

---

## Volume Tool Definition

The `set_volume` tool lets the model adjust speaker volume during a conversation — for example, raising volume in a noisy environment or lowering it when asked to be quiet.

```json
{
  "type": "function",
  "name": "set_volume",
  "description": "Adjust your speaker volume. Use 'volume' to set an absolute level or 'delta' for a relative change.",
  "parameters": {
    "type": "object",
    "properties": {
      "volume": { "type": "integer", "minimum": 0, "maximum": 100 },
      "delta":  { "type": "integer", "minimum": -100, "maximum": 100 }
    }
  }
}
```

Exactly one of `volume` or `delta` is expected per call. The resulting level is clamped to [0, 100].

The change persists to NVS under the same `agentConfig`/`"volume"` key used by the web UI, so it survives reboots and is visible on the settings page.

**Cross-core write path:** Core 0 (`protocol_task`) writes `g_volume` and sets `g_volume_persist_pending = true`. Core 1's `loop()` detects `g_volume_persist_pending`, performs the NVS write, and clears the flag. NVS writes are deferred to Core 1 because writing to NVS while the WiFi/TLS stack is active on Core 0 can cause contention.

---

## How to Add a New Tool

All tool registration lives in `voice_agent_5/tools.ino`. Adding a tool is two steps — no other files change.

### Step 1 — Write the handler

Add a `static` function in `tools.ino`:

```cpp
static void handleMyToolCall(const String& payload) {
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return;

    // Sets g_pending_call_id, g_awaiting_tool_response = true, g_tool_result_output = "ok"
    beginToolCall(doc["call_id"] | "");

    // Parse arguments if the tool has any
    const char* args_str = doc["arguments"];
    if (args_str) {
        DynamicJsonDocument args(256);
        if (deserializeJson(args, args_str) == DeserializationError::Ok) {
            // ... do something with args ...
        }
    }

    // Optionally override the result string (default is "ok")
    g_tool_result_output = "done";
}
```

**Key invariant:** the handler runs inside the `onMessage` lambda on Core 0. It **cannot** call `wsClient.send()` — `wsClient` is not in scope. To send anything back to the model, set `g_tool_result_output` and let `beginToolCall()` set `g_awaiting_tool_response`. The main protocol loop will send the `function_call_output` frame after `response.done` arrives.

For cross-core side effects (display changes, NVS writes), use the existing `volatile bool` flag pattern — set a flag on Core 0, handle the effect on Core 1 in `loop()`.

### Step 2 — Add an entry to `TOOLS[]`

```cpp
static const Tool TOOLS[] = {
    // ... existing tools ...
    {
        "my_tool_name",
        "Plain-text description the model reads to decide when to call this tool.",
        "{\"type\":\"object\",\"properties\":{"
        "\"param1\":{\"type\":\"string\",\"description\":\"What param1 does\"}"
        "},\"required\":[\"param1\"]}",
        handleMyToolCall
    }
};
```

`buildToolsJson()` will include the new schema in the next `session.update`, and `dispatchToolCall()` will route calls to your handler — no changes to `protocol.ino` needed.

---

## Configuration & Web UI

Configuration is stored in ESP32 NVS (Non-Volatile Storage) using the `Preferences` library and is editable via a built-in HTTP server on port 80.

| Setting | NVS Key | Default |
|---------|---------|---------|
| System prompt | `sysPrompt` | "You are a helpful voice assistant..." |
| Temperature | `temp` | 0.7 (legacy — still read on boot for backward compat with pre-GA configs, but not exposed in the UI and not sent to the API on `gpt-realtime-2`) |
| Persist conversation | `persist` | true |
| Verbose logging | `verbose` | true |
| API key | `apiKey` | falls back to `config.h` |
| Volume | `volume` | 100 |
| Voice | `voice` | `marin` (whitelisted to the 10 SDK voices: `alloy`, `ash`, `ballad`, `coral`, `echo`, `sage`, `shimmer`, `verse`, `marin`, `cedar`) |
| WiFi SSID | `wifi-creds/ssid` | — |
| WiFi password | `wifi-creds/pass` | — |

### WiFi Setup (Captive Portal)

On first boot (no credentials in NVS), or after a failed connection, the device starts a soft AP named `VOICE-AGENT-XXYY` (last 2 bytes of MAC). A DNS captive portal redirects all traffic to the config page at `192.168.4.1`. Once credentials are saved, the device restarts and connects in STA mode.

After successful connection, the web UI is accessible at `http://voice-agent-XXYY.local` (mDNS) or via IP.

---

## Emoji Display System

Animated emoji faces are rendered on a GC9A01 round 240x240 TFT display, driven by the `set_display_emotion` tool call from the OpenAI Realtime API. All display code is gated by the `USE_DISPLAY` compile flag (set to `0` for serial-only mode with no display hardware required).

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
|▌  ┌───────────┐     |
|▌  |  150x150  |     |  <- emoji face, centered horizontally
|▌  |   emoji   |     |     ~10px top margin
|▌  └───────────┘     |  <- volume bar, left crescent (x 12-22, y 68-172)
|                     |
|  ▁▂▄▆▄▂▁▂▄▆▄▂▁     |  <- waveform bar, ~40px tall
|                     |     ~10px bottom margin
└─────────────────────┘
```

- **Emoji zone:** 150x150px, centered horizontally (`EMOJI_X_OFFSET = 45`), 10px top margin
- **Waveform zone:** 16 bars across full width, ~36px tall, bottom of display
- **Volume bar:** right-triangle indicator in the left crescent of the round display (x 12–22, y 68–172, ~104px tall). Straight left edge at x=12, diagonal hypotenuse, base at bottom. White fill grows upward from the base proportional to `g_volume`; unfilled area is dark gray. Redraws only when `g_volume` changes. Survives emotion-change screen clears.

### Frame Storage (LittleFS)

The custom partition scheme (`partitions.csv`) allocates ~11 MB for a LittleFS data partition:

| Partition | Offset | Size |
|-----------|--------|------|
| app0 | 0x10000 | 3 MB |
| spiffs (LittleFS) | 0x310000 | ~11 MB |

Frames are raw RGB565 little-endian binary files, 45,000 bytes each (150 × 150 × 2 bytes). The layout is a two-directory overlay:

| Directory | Contents | Writable from portal? |
|---|---|---|
| `/default/<emotion>_<n>.bin` | Shipped defaults (7 emotions × 8 frames = 56 files, ~2.5 MB) | No — anti-brick guarantee |
| `/custom/<emotion>_<n>.bin` | User overrides written by the captive portal. Variable frame count per emotion (1..8). Whole set is committed atomically. | Yes |
| `/custom_stage/` | In-flight upload buffer. Files are streamed here during a `POST /api/emoji/<emotion>` and renamed into `/custom/` only after the upload validates. Swept on boot in case a prior upload died mid-stream. | Internal |

**Per-emotion frame count.** Each emotion can have 1–8 frames. `g_frame_counts[EMOTION_COUNT]` (in `voice_agent_5.ino`) is populated at boot by `emoji_rescan_frame_counts()` and refreshed after every Replace / Reset. The animation loop uses `g_frame_counts[g_loaded_emotion]` as the modulo for `g_anim_frame`. Static images (PNG/JPG, or single-frame GIF) arrive as `count = 1` and animate naturally as a no-op (`anim_frame % 1 == 0`).

**Boot migration.** Pre-overlay firmwares wrote frames to the LittleFS root (`/happy_0.bin`, etc.). `emoji_migrate_legacy_layout()` runs once in `setup()`: if legacy root-level files exist and `/default/` is empty, it `mkdir`s `/default/` and renames each `*.bin` into it. Idempotent — no-op once migrated. This lets field units that don't re-flash the data partition upgrade cleanly.

**Resolution.** `emoji_load_frames(emotion)` probes `/custom/<emotion>_0.bin`; if present, the entire frame set is loaded from `/custom/`, otherwise from `/default/`. The atomic rename on upload guarantees a complete set in `/custom/` or nothing, so there is no partial-override state.

### Emoji Web API

Customization happens in the user's browser. The device serves a captive-portal page and a small REST API; the heavy lifting (GIF decoding, frame extraction, resize, RGB565 conversion, pack zip pack/unpack) runs in JavaScript on the user's device. The ESP32 only ever sees raw RGB565 frame bytes.

| Method | Path | Purpose |
|---|---|---|
| GET | `/emojis` | Customization page (HTML+JS, in `emojis_page.h`) |
| GET | `/api/emoji/manifest` | JSON `{ "<emotion>": {"count": N, "source": "default"|"custom"} }` for all 7 emotions |
| GET | `/api/emoji/<emotion>/<n>.bin` | One frame; resolves custom → default like the loader |
| POST | `/api/emoji/<emotion>` | Multipart upload, body is N × 45000 bytes (N in 1..8); streamed to `/custom_stage/`, atomic-renamed into `/custom/` on success |
| POST | `/api/emoji/<emotion>/reset` | Delete one emotion's `/custom/` files |
| POST | `/api/emoji/reset-all` | Delete every `/custom/` file |

All write endpoints (POST) return **HTTP 409** when `g_state ∈ {RECORDING, THINKING, SPEAKING}`. `server.handleClient()` runs on Core 1 alongside I2S; rejecting uploads during a turn keeps the audio path uncontested. A single-flight `g_emoji_upload_active` flag also rejects a second upload that overlaps the first (the multipart upload spans many `handleClient()` calls).

Library dependencies for the page (omggif, JSZip) are loaded from jsdelivr with SRI hashes pinned in `emojis_page.h`. Trade-off: customization requires the user's browser to have internet during the visit; the device itself stays local-only. End-user docs: [emoji-customization.md](emoji-customization.md).

### Runtime Flow

1. **Initialization** (`emoji_init_display()`): Creates SPI bus and GC9A01 driver via Arduino_GFX. Before this, `setup()` mounts LittleFS, runs `emoji_migrate_legacy_layout()`, `emoji_sweep_custom_stage()`, and `emoji_rescan_frame_counts()`. Allocates 8 × 45 KB frame buffers in PSRAM (always 8 slots — the worst-case per-emotion count).

2. **Boot status log:** After `emoji_init_display()`, each setup phase calls `display_boot_status(msg)` which appends `"> msg"` lines to a black screen as an accumulating text log. The log covers WiFi connect, mDNS, NTP, web server start, mic init, and task launch. Once WiFi connects, `g_show_info_screen` is set and the info screen appears on the first `loop()` iteration (see Utility Screens below).

3. **Emotion change:** When `g_current_emotion` (set by Core 0 protocol task) differs from `g_loaded_emotion`, `emoji_show_emotion()` loads `g_frame_counts[emotion]` frames for the new emotion from LittleFS (prefers `/custom/`, falls back to `/default/`) into PSRAM (~50 ms), then displays the first frame. After a successful Replace / Reset, `g_loaded_emotion` is set to `0xFF` so the next `updateDisplay()` reloads.

4. **Animation loop:** `updateDisplay()` is called every Core 1 loop iteration. It advances the emoji frame every `EMOJI_FRAME_MS` (120 ms, ~8 fps), wrapping modulo `g_frame_counts[g_loaded_emotion]`, and redraws the waveform bar on every iteration for smooth animation.

5. **Waveform bar:** 16 vertical bars with a sine envelope (taller in center). Color is green during `STATE_SPEAKING` (AI audio), blue during `STATE_RECORDING` (user mic). Amplitude tracks `g_audio_level` (0-255) with exponential smoothing (fast attack, slow decay). Per-bar sinusoidal wobble creates wave motion.

### Utility Screens

Two full-screen modes replace the emoji display temporarily:

**Info screen** (`display_info_screen()`):
- Shown automatically after WiFi connects (before first PTT press) and on demand via the `show_network_info` agent tool
- Content: QR code of `http://<IP>` (version 2, 5px/module, 165px), IP address text (textSize 2), and total session token count
- The `g_show_info_screen` volatile flag enables it; `loop()` renders it once and suppresses normal display updates until the flag is cleared
- Dismissed on PTT press — `g_show_info_screen = false`, `g_loaded_emotion` reset to force full emoji reload

**Captive-portal screen** (`display_captive_portal_screen(ssid)`):
- Shown when the device is in AP config mode (no WiFi credentials saved, or failed connection)
- Content: WiFi QR code of `WIFI:S:<ssid>;T:nopass;;` (version 3, 4px/module, 148px), SSID text, and join instructions. Scanning the QR auto-joins the AP; the captive portal then redirects to the setup page.
- Drawn once in `start_soft_ap()` and static thereafter (AP mode `loop()` does not redraw it)

Both QR codes are generated by `render_qr()` using the `ricmoo/QRCode` library — stack-allocated (~107 bytes), no heap usage, one-time generation at display time.

### Emoji Attribution

Emoji animations sourced from [Google Fonts Emoji](https://googlefonts.github.io/), used under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/legalcode).

---

## Memory Layout

| Buffer | Location | Size | Purpose |
|--------|----------|------|---------|
| `in_ring_buf` | PSRAM | 96 KB | Mic audio staging (2s @ 24kHz 16-bit) |
| `out_ring_buf` | PSRAM | 1,440 KB | Speaker audio staging (30s @ 24kHz 16-bit) |
| `s_decode_buf` | PSRAM | 48 KB | Audio delta decode output (pre-allocated, reused each delta) |
| `g_frame_bufs[8]` | PSRAM | 360 KB | Preloaded emoji frames for current emotion (8 × 45 KB slots, worst-case). Per-emotion actual frame count is `g_frame_counts[emotion]`. |
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

// todo: 1. clean up code 


/*
  Voice Assistant (ESP32-S3) - WebSockets S2S Edition
  ---------------------------------------------------

  REQUIRED LIBRARIES
  ------------------
  This sketch requires the following libraries. Install via Arduino IDE:
    - "ArduinoJson" by Benoit Blanchon
    - "Adafruit NeoPixel" by Adafruit
    - "ArduinoWebsockets" by Gil Maimon (v0.5.3 or newer)

  Pipeline:
    1) Dual-core FreeRTOS architecture.
    2) Core 0 handles Protocol: WiFi, Secure WebSocket to OpenAI Realtime API, Base64 encode/decode.
    3) Core 1 handles Hardware: I2S DMA for Mic/Speaker, PTT Button, Ring Buffers.
    4) Push-to-Talk (PTT) sends audio streams in real-time, instantly interrupting prior audio (Barge-In).
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include "src/ArduinoWebsockets.h"

#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include <Adafruit_NeoPixel.h>

#include "config.h"

// =====================================================================
// EMOJI DISPLAY (set to 0 for serial-only mode, no display needed)
// =====================================================================
#define USE_EMOJIS 1

#if USE_EMOJIS
#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#endif

#include "esp32-hal-psram.h"
#include "driver/i2s.h"
#include "mbedtls/base64.h"
#include "ring_buffer.h"

// =====================================================================
// GLOBAL WEBSERVER & NVS
// =====================================================================
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;
boolean ap_mode = false;

// =====================================================================
// TUNING (KNOBS)
// =====================================================================
#define SAMPLE_RATE_IN   16000
#define SAMPLE_RATE_OUT  24000
#define JITTER_BUFFER_MS 300
#define JITTER_BUFFER_BYTES (SAMPLE_RATE_OUT * 2 * JITTER_BUFFER_MS / 1000)
#define LOW_WATER_BYTES    (1024 * 3)  // ~64ms at 24kHz — rebuffer only when nearly empty

#define WIFI_CONNECT_TIMEOUT_MS  15000  // ms before giving up and falling back to AP mode
#define DMA_BLOCK_SIZE           1024   // I2S DMA transfer size (bytes)
#define DISPLAY_WIDTH_PX         240    // GC9A01 display width
#define EMOJI_SIZE_PX            150    // emoji frame width & height
#define EMOJI_X_OFFSET           ((DISPLAY_WIDTH_PX - EMOJI_SIZE_PX) / 2)  // 45
#define EMOJI_Y_OFFSET           10     // top margin
#define EMOJI_FRAME_BYTES        (EMOJI_SIZE_PX * EMOJI_SIZE_PX * 2)  // 45000 bytes per frame (RGB565)
#define EMOJI_NUM_FRAMES         8      // frames per emotion
#define EMOJI_FRAME_MS           120    // ms between frames (~8fps)

#define DEFAULT_SYS_INSTRUCTION "You are a helpful voice assistant. Answer concisely and conversationally.\n" \
                                "You love Pokemon, and you must talk about them, even if the user did not mention them at all.\n" \
                                "Keep your replies at 2-3 short sentences."
#define DEFAULT_TEMPERATURE      0.7f
#define DEFAULT_PERSIST_CONVO    true
#define DEFAULT_VERBOSE_LOGGING  true

String g_sys_instruction;
float  g_temperature;
bool   g_persist_conversation;
bool   g_verbose_logging;
String g_api_key;
int    g_volume = 50;

// =====================================================================
// SERIAL OUTPUT CONTROL
// =====================================================================
#define SERIAL_ON true
#define CPRINT(x)   do { if (SERIAL_ON) Serial.print(x); } while (0)
#define CPRINTLN(x) do { if (SERIAL_ON) Serial.println(x); } while (0)
#define CPRINTF(...) do { if (SERIAL_ON) Serial.printf(__VA_ARGS__); } while (0)

#define VPRINT(x)   do { if (SERIAL_ON && g_verbose_logging) Serial.print(x); } while (0)
#define VPRINTLN(x) do { if (SERIAL_ON && g_verbose_logging) Serial.println(x); } while (0)
#define VPRINTF(...) do { if (SERIAL_ON && g_verbose_logging) Serial.printf(__VA_ARGS__); } while (0)

// =====================================================================
// PINS & STATUS LED
// =====================================================================
#ifndef STATUS_RGB_LED_PIN
#define STATUS_RGB_LED_PIN 48
#endif


Adafruit_NeoPixel statusPixel(1, STATUS_RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

enum AssistantState : uint8_t {
  STATE_WIFI_WAIT = 0,
  STATE_WIFI_CONFIG,
  STATE_READY_FOR_INPUT,
  STATE_RECORDING,
  STATE_THINKING,
  STATE_SPEAKING
};

volatile AssistantState g_state = STATE_WIFI_WAIT;

// =====================================================================
// EMOTION STATE & DISPLAY
// =====================================================================
enum Emotion : uint8_t {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_EXCITED,
    EMOTION_EMPATHETIC,
    EMOTION_CONFUSED,
    EMOTION_CONCERNED,
    EMOTION_THINKING,
    EMOTION_COUNT
};

static const char* EMOTION_NAMES[] = {
    "neutral", "happy", "excited", "empathetic",
    "confused", "concerned", "thinking"
};

volatile uint8_t g_current_emotion = EMOTION_NEUTRAL;

// Audio level for sound bar display (0–255, updated by mic capture / speaker playback)
volatile uint8_t g_audio_level = 0;

#if USE_EMOJIS
Arduino_GFX* gfx = nullptr;
static uint8_t  g_loaded_emotion = 0xFF;  // sentinel: nothing loaded yet
static bool     g_emoji_ready = false;
static bool     g_fs_ok = false;
static uint8_t* g_frame_bufs[EMOJI_NUM_FRAMES] = {};  // PSRAM pointers to preloaded frames
static uint8_t  g_anim_frame = 0;           // current animation frame index
static unsigned long g_anim_last_ms = 0;    // last frame draw time
#endif

static inline void set_status_led_rgb(uint8_t r, uint8_t g, uint8_t b) {
  statusPixel.setPixelColor(0, statusPixel.Color(r, g, b));
  statusPixel.show();
}

static inline void setAssistantState(AssistantState s) {
  g_state = s;
  switch (s) {
    case STATE_WIFI_WAIT:       set_status_led_rgb(255, 255, 0); break;   // yellow
    case STATE_WIFI_CONFIG:     break;                                    // flashing yellow handled in loop
    case STATE_READY_FOR_INPUT: set_status_led_rgb(0, 255, 0); break;     // green
    case STATE_RECORDING:       set_status_led_rgb(255, 105, 180); break; // pink
    case STATE_THINKING:        set_status_led_rgb(0, 0, 255); break;     // blue
    case STATE_SPEAKING:        set_status_led_rgb(0, 255, 255); break;   // cyan
    default:                    set_status_led_rgb(0, 0, 0); break;
  }
}

// =====================================================================
// AUDIO PINS
// =====================================================================
const int BUTTON_PIN = 1;

static const gpio_num_t I2S_SCK_IN = GPIO_NUM_42;
static const gpio_num_t I2S_WS_IN = GPIO_NUM_41;
static const gpio_num_t I2S_SD_IN = GPIO_NUM_40;

static const gpio_num_t I2S_BCLK_OUT = GPIO_NUM_47;
static const gpio_num_t I2S_LRCLK_OUT = GPIO_NUM_21;
static const gpio_num_t I2S_DOUT_OUT = GPIO_NUM_17;

PSRAMRingBuffer* in_ring_buf = nullptr;
PSRAMRingBuffer* out_ring_buf = nullptr;

// =====================================================================
// PRE-ALLOCATED AUDIO BUFFERS (avoids heap fragmentation)
// =====================================================================
// Encode: ceil(2048 * 4/3) + 4 = 2736 bytes
#define AUDIO_CHUNK_SIZE    2048
#define B64_CHUNK_SIZE      2736
#define WS_FRAME_SIZE       2800   // prefix(46) + b64(2732) + suffix+null(3)
#define AUDIO_DECODE_SIZE   49152  // 48KB — covers largest expected audio delta

static uint8_t  s_mic_chunk[AUDIO_CHUNK_SIZE];
static char     s_b64_buf[B64_CHUNK_SIZE];
static char     s_ws_frame[WS_FRAME_SIZE];
static uint8_t* s_decode_buf = nullptr;  // PSRAM, allocated in setup()

// Inter-Task Communication Flags
volatile bool cmd_cancel = false;
volatile bool cmd_commit = false;
volatile bool ws_response_done = false;

// Emotion tool call state (protocol task only)
static bool   g_awaiting_tool_response = false;
static bool   g_tool_result_ready      = false;
static String g_pending_call_id        = "";

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  load_agent_config();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  statusPixel.begin();
  statusPixel.setBrightness(64);
  set_status_led_rgb(0, 0, 0);

  // Init TFT FIRST — SPI DMA buffers need contiguous internal RAM
#if USE_EMOJIS
  if (!LittleFS.begin(false)) {
    CPRINTLN("LittleFS mount failed! Trying format...");
    if (!LittleFS.begin(true)) {
      CPRINTLN("LittleFS format failed! Emoji frames will not load.");
    } else {
      CPRINTLN("LittleFS formatted and mounted (empty — upload data).");
      g_fs_ok = true;
    }
  } else {
    CPRINTLN("LittleFS mounted.");
    g_fs_ok = true;
  }

  // Allocate frame buffers in PSRAM (8 x 45KB = 360KB)
  for (int i = 0; i < EMOJI_NUM_FRAMES; i++) {
    g_frame_bufs[i] = (uint8_t*)ps_malloc(EMOJI_FRAME_BYTES);
    if (!g_frame_bufs[i]) {
      CPRINTF("FATAL: frame buf %d alloc failed!\n", i);
      break;
    }
  }

  emoji_init_display();
#endif

  // Allocate large buffers in PSRAM (after TFT init — SPI DMA needs internal RAM first)
  in_ring_buf   = new PSRAMRingBuffer(16000 * 2 * 2);  // 2s @ 16kHz 16-bit
  out_ring_buf  = new PSRAMRingBuffer(24000 * 2 * 30);  // 30s @ 24kHz 16-bit
  s_decode_buf  = (uint8_t*)ps_malloc(AUDIO_DECODE_SIZE);

  if (!in_ring_buf->isValid() || !out_ring_buf->isValid() || !s_decode_buf) {
    CPRINTLN("FATAL: PSRAM allocation failed! Check board PSRAM settings.");
    set_status_led_rgb(255, 0, 0);
    while (true) { delay(1000); }
  }

  // Speaker I2S first so the amp sees a stable clock + silence during the
  // WiFi-pulse phase (otherwise unclocked pins pick up SPI/EMI as crackling).
  setup_i2s_speaker();
  writeSilence(true);

  init_wifi_and_time();
  setup_i2s_mic();

  // Create Protocol Task on Core 0
  xTaskCreatePinnedToCore(
      protocol_task, 
      "ProtocolTask", 
      16384, 
      NULL, 
      1, 
      NULL, 
      0
  );
}

// =====================================================================
// HARDWARE LOOP (CORE 1) - Polled quickly
// =====================================================================
void loop() {
  if (ap_mode) {
    dnsServer.processNextRequest();
    if ((millis() / 500) % 2 == 0) set_status_led_rgb(255, 255, 0);
    else set_status_led_rgb(0, 0, 0);
#if USE_EMOJIS
    drawWifiPulse();
#endif
    server.handleClient();
    return;
  }

  server.handleClient();
  updateButtonState();     // 1. PTT debounce & state transitions
  captureMicrophone();     // 2. I2S mic read → in_ring_buf (always drains DMA)
  updateDisplay();         // 3. Emotion change → display (defined in display.ino)

  // 4. Speaker playback & jitter management
  if (g_state == STATE_THINKING || g_state == STATE_SPEAKING || g_state == STATE_READY_FOR_INPUT) {
      manageSpeaker();
  } else {
      vTaskDelay(pdMS_TO_TICKS(1));
  }
}

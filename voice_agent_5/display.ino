// =====================================================================
// EMOJI DISPLAY — init, frame loading from LittleFS, animation
// =====================================================================
#if USE_EMOJIS

// Load all 8 frames for a given emotion from LittleFS into PSRAM buffers
static bool emoji_load_frames(uint8_t emotion) {
    if (!g_fs_ok) return false;
    if (emotion >= EMOTION_COUNT) return false;

    const char* name = EMOTION_NAMES[emotion];
    char path[40];

    for (int i = 0; i < EMOJI_NUM_FRAMES; i++) {
        if (!g_frame_bufs[i]) return false;

        snprintf(path, sizeof(path), "/%s_%d.bin", name, i);
        File f = LittleFS.open(path, "r");
        if (!f) {
            CPRINTF("Display: missing %s\n", path);
            return false;
        }
        size_t got = f.read(g_frame_bufs[i], EMOJI_FRAME_BYTES);
        f.close();
        if (got != EMOJI_FRAME_BYTES) {
            CPRINTF("Display: short read %s (%u/%u)\n", path, got, EMOJI_FRAME_BYTES);
            return false;
        }
    }
    CPRINTF("Display: loaded %d frames for '%s'\n", EMOJI_NUM_FRAMES, name);
    return true;
}

// Draw a single frame at the emoji position
static void emoji_draw_frame(uint8_t frame_idx) {
    if (frame_idx >= EMOJI_NUM_FRAMES || !g_frame_bufs[frame_idx]) return;
    gfx->draw16bitRGBBitmap(
        EMOJI_X_OFFSET, EMOJI_Y_OFFSET,
        (uint16_t*)g_frame_bufs[frame_idx],
        EMOJI_SIZE_PX, EMOJI_SIZE_PX
    );
}

// Switch to a new emotion: load frames, draw first frame immediately
static void emoji_show_emotion(uint8_t emotion) {
    if (!g_emoji_ready) return;
    if (emotion >= EMOTION_COUNT) emotion = EMOTION_NEUTRAL;
    if (emotion == g_loaded_emotion) return;

    const char* name = EMOTION_NAMES[emotion];

    if (g_fs_ok && emoji_load_frames(emotion)) {
        // Frames loaded — show first frame
        g_loaded_emotion = emotion;
        g_anim_frame = 0;
        g_anim_last_ms = millis();
        gfx->fillScreen(RGB565_BLACK);
        emoji_draw_frame(0);
        CPRINTF("Display: %s (animated)\n", name);
    } else {
        // Fallback: show text name (no LittleFS or missing files)
        g_loaded_emotion = emotion;
        g_anim_frame = 0;
        gfx->fillScreen(RGB565_BLACK);
        gfx->setTextColor(RGB565_WHITE);
        gfx->setTextSize(3);
        int len = strlen(name);
        int tx = (DISPLAY_WIDTH_PX - len * 18) / 2;
        if (tx < 0) tx = 0;
        gfx->setCursor(tx, 105);
        gfx->print(name);
        CPRINTF("Display: %s (text fallback)\n", name);
    }
}

static void emoji_init_display() {
    CPRINTLN(">>   creating display bus..."); Serial.flush();
    Arduino_DataBus *bus = new Arduino_ESP32SPI(
        4 /* DC */, 5 /* CS */, 6 /* SCLK */, 7 /* MOSI */, -1 /* MISO */);
    gfx = new Arduino_GC9A01(bus, 2 /* RST */, 0 /* rotation */, true /* IPS */);

    CPRINTLN(">>   gfx->begin()..."); Serial.flush();
    if (!gfx->begin()) {
        CPRINTLN("Display: gfx->begin() FAILED!");
        return;
    }
    gfx->fillScreen(RGB565_BLACK);
    g_emoji_ready = true;

    // Don't show emoji yet — WiFi pulse will display first.
    // Emoji loads on first updateDisplay() call after clearWifiPulse().
    CPRINTLN("Display: GC9A01 initialized");
}

// ---- Pulsing blue circle (WiFi wait) ----------------------------------

static int16_t s_pulse_prev_r = 0;   // previous circle radius (for erase)

// Call from blocking WiFi loops to show a breathing blue circle.
// Safe to call every ~30-50ms.
static void drawWifiPulse() {
    if (!g_emoji_ready) return;

    // Breathing sine: period ~2s, radius 20..90
    float phase = (float)(millis() % 2000) / 2000.0f * 2.0f * PI;
    int16_t r = (int16_t)(55.0f + 35.0f * sinf(phase));

    if (r == s_pulse_prev_r) return;

    int16_t cx = DISPLAY_WIDTH_PX / 2;
    int16_t cy = DISPLAY_WIDTH_PX / 2;

    // Erase old circle, draw new
    if (s_pulse_prev_r > 0) {
        gfx->fillCircle(cx, cy, s_pulse_prev_r, RGB565_BLACK);
    }
    gfx->fillCircle(cx, cy, r, 0x001F);   // pure blue RGB565
    s_pulse_prev_r = r;
}

// Called once when WiFi connects — clears the pulse and resets for emoji display
static void clearWifiPulse() {
    if (!g_emoji_ready) return;
    if (s_pulse_prev_r > 0) {
        gfx->fillScreen(RGB565_BLACK);
        s_pulse_prev_r = 0;
    }
    // Force emoji reload on next updateDisplay()
    g_loaded_emotion = 0xFF;
}

// ---- Waveform bar -----------------------------------------------------

// Geometry — waveform zone below the emoji on the round display
#define WF_NUM_BARS    16            // number of vertical bars
#define WF_BAR_W       6            // bar width in pixels
#define WF_BAR_GAP     2            // gap between bars
#define WF_MAX_H       36           // max bar height in pixels
#define WF_TOTAL_W     (WF_NUM_BARS * WF_BAR_W + (WF_NUM_BARS - 1) * WF_BAR_GAP)  // 126
#define WF_X           ((DISPLAY_WIDTH_PX - WF_TOTAL_W) / 2)   // 57
#define WF_BOTTOM      215          // bottom edge of waveform zone
#define WF_TOP         (WF_BOTTOM - WF_MAX_H)                  // 179

// RGB565 colours (per integration doc: green=AI speaking, blue=user recording)
#define WF_COLOR_REC   0x001F       // blue  (user recording)
#define WF_COLOR_PLAY  0x07E0       // green (AI speaking)
#define WF_COLOR_BG    0x0000       // black

static uint8_t  s_smoothed_level = 0;
static uint8_t  s_prev_bar_h[WF_NUM_BARS] = {};   // previous bar heights for delta redraw
static bool     s_wf_visible = false;

// Sine envelope: bars are tallest in the centre, shorter at edges
static const float WF_ENVELOPE[WF_NUM_BARS] = {
    0.30f, 0.50f, 0.65f, 0.80f, 0.90f, 0.97f, 1.00f, 1.00f,
    1.00f, 1.00f, 0.97f, 0.90f, 0.80f, 0.65f, 0.50f, 0.30f
};

static void drawWaveform() {
    if (!g_emoji_ready) return;

    bool active = (g_state == STATE_RECORDING || g_state == STATE_SPEAKING);

    if (!active) {
        if (s_wf_visible) {
            gfx->fillRect(WF_X, WF_TOP, WF_TOTAL_W, WF_MAX_H, WF_COLOR_BG);
            memset(s_prev_bar_h, 0, sizeof(s_prev_bar_h));
            s_smoothed_level = 0;
            s_wf_visible = false;
        }
        return;
    }

    // Exponential smoothing (fast attack, slower decay)
    uint8_t raw = g_audio_level;
    if (raw > s_smoothed_level) {
        s_smoothed_level = raw;
    } else {
        s_smoothed_level = (uint8_t)((s_smoothed_level * 3 + raw) / 4);
    }

    uint16_t color = (g_state == STATE_RECORDING) ? WF_COLOR_REC : WF_COLOR_PLAY;

    // Animate: time-varying per-bar wobble so bars dance independently
    float t = (float)(millis() % 3000) / 3000.0f * 2.0f * PI;

    for (int i = 0; i < WF_NUM_BARS; i++) {
        // Per-bar oscillation offset creates wave motion
        float wobble = 0.7f + 0.3f * sinf(t + i * 0.9f);
        uint8_t h = (uint8_t)(((uint16_t)s_smoothed_level * WF_MAX_H / 255)
                               * WF_ENVELOPE[i] * wobble);
        if (h < 2) h = (s_smoothed_level > 10) ? 2 : 0;  // minimum visible bar

        if (h == s_prev_bar_h[i]) continue;

        int16_t bx = WF_X + i * (WF_BAR_W + WF_BAR_GAP);

        if (h > s_prev_bar_h[i]) {
            // Bar grew — draw the new portion above the old top
            int16_t old_top = WF_BOTTOM - s_prev_bar_h[i];
            int16_t new_top = WF_BOTTOM - h;
            gfx->fillRect(bx, new_top, WF_BAR_W, old_top - new_top, color);
        } else {
            // Bar shrank — erase the old portion above the new top
            int16_t old_top = WF_BOTTOM - s_prev_bar_h[i];
            int16_t new_top = WF_BOTTOM - h;
            gfx->fillRect(bx, old_top, WF_BAR_W, new_top - old_top, WF_COLOR_BG);
        }
        s_prev_bar_h[i] = h;
    }

    s_wf_visible = true;
}

// Called from loop() — updates display when emotion changes, advances animation
static void updateDisplay() {
    uint8_t emo = g_current_emotion;

    // Emotion changed — load new frames
    if (emo != g_loaded_emotion) {
        emoji_show_emotion(emo);
        memset(s_prev_bar_h, 0, sizeof(s_prev_bar_h));  // force full waveform redraw
        s_wf_visible = false;
        return;
    }

    // Animate: advance frame if enough time has passed and we have loaded frames
    if (g_fs_ok && g_emoji_ready && g_loaded_emotion < EMOTION_COUNT) {
        unsigned long now = millis();
        if (now - g_anim_last_ms >= EMOJI_FRAME_MS) {
            g_anim_last_ms = now;
            g_anim_frame = (g_anim_frame + 1) % EMOJI_NUM_FRAMES;
            emoji_draw_frame(g_anim_frame);
        }
    }

    // Waveform — updated every loop iteration for smooth animation
    drawWaveform();
}

#else

// Stub so loop() can call updateDisplay() unconditionally
static void updateDisplay() {}

#endif // USE_EMOJIS

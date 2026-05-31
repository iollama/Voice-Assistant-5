// =====================================================================
// EMOJI DISPLAY — init, frame loading from LittleFS, animation
// =====================================================================
#if USE_DISPLAY

// Bundled espressif esp_qrcode API from arduino-esp32 (esp_qrcode_generate,
// esp_qrcode_get_module, ESP_QRCODE_ECC_LOW). Used by render_qr() below.
#include <qrcode.h>

// Count consecutive frame files /<dir>/<emotion>_<n>.bin starting at n=0. Stops on first gap.
static uint8_t emoji_count_frames_in_dir(const char* dir, const char* name) {
    char path[EMOJI_PATH_MAX];
    uint8_t count = 0;
    for (int i = 0; i < EMOJI_NUM_FRAMES; i++) {
        snprintf(path, sizeof(path), "%s/%s_%d.bin", dir, name, i);
        if (!LittleFS.exists(path)) break;
        count++;
    }
    return count;
}

// Populate g_frame_counts[] for every emotion. Prefer /custom/ count; fall back to /default/.
// Called once at boot and after every Replace / Reset / Reset-all.
void emoji_rescan_frame_counts() {
    for (int e = 0; e < EMOTION_COUNT; e++) {
        uint8_t count = emoji_count_frames_in_dir("/custom", EMOTION_NAMES[e]);
        if (count == 0) {
            count = emoji_count_frames_in_dir("/default", EMOTION_NAMES[e]);
        }
        g_frame_counts[e] = count;
    }
    CPRINTF("Emoji counts: ");
    for (int e = 0; e < EMOTION_COUNT; e++) {
        CPRINTF("%s=%u ", EMOTION_NAMES[e], g_frame_counts[e]);
    }
    CPRINTLN("");
}

// One-shot migration: pre-/default/ firmwares wrote *.bin to the LittleFS root.
// If we find legacy files AND /default/ has not been populated yet, move them in.
// Idempotent — no-op once migrated.
void emoji_migrate_legacy_layout() {
    char src[EMOJI_PATH_MAX], dst[EMOJI_PATH_MAX];

    bool legacy_exists = false;
    for (int e = 0; e < EMOTION_COUNT && !legacy_exists; e++) {
        snprintf(src, sizeof(src), "/%s_0.bin", EMOTION_NAMES[e]);
        if (LittleFS.exists(src)) legacy_exists = true;
    }
    if (!legacy_exists) return;

    snprintf(dst, sizeof(dst), "/default/%s_0.bin", EMOTION_NAMES[0]);
    if (LittleFS.exists(dst)) return;   // new layout already populated; leave legacy files alone

    LittleFS.mkdir("/default");
    int moved = 0;
    for (int e = 0; e < EMOTION_COUNT; e++) {
        for (int i = 0; i < EMOJI_NUM_FRAMES; i++) {
            snprintf(src, sizeof(src), "/%s_%d.bin", EMOTION_NAMES[e], i);
            snprintf(dst, sizeof(dst), "/default/%s_%d.bin", EMOTION_NAMES[e], i);
            if (LittleFS.exists(src) && LittleFS.rename(src, dst)) moved++;
        }
    }
    CPRINTF("Migrated %d legacy emoji files into /default/\n", moved);
}

// Delete any leftover staged files from a previous boot that died mid-upload.
void emoji_sweep_custom_stage() {
    char path[EMOJI_PATH_MAX];
    for (int e = 0; e < EMOTION_COUNT; e++) {
        for (int i = 0; i < EMOJI_NUM_FRAMES; i++) {
            snprintf(path, sizeof(path), "/custom_stage/%s_%d.bin", EMOTION_NAMES[e], i);
            LittleFS.remove(path);
        }
    }
}

// Load frames for emotion. Prefers /custom/ (entire set or nothing — enforced by upload
// atomic rename), falls back to /default/.
static bool emoji_load_frames(uint8_t emotion) {
    if (!g_fs_ok) return false;
    if (emotion >= EMOTION_COUNT) return false;

    const char* name = EMOTION_NAMES[emotion];
    uint8_t count = g_frame_counts[emotion];
    if (count == 0 || count > EMOJI_NUM_FRAMES) return false;

    char probe[EMOJI_PATH_MAX];
    snprintf(probe, sizeof(probe), "/custom/%s_0.bin", name);
    const char* dir = LittleFS.exists(probe) ? "/custom" : "/default";

    char path[EMOJI_PATH_MAX];
    for (int i = 0; i < count; i++) {
        if (!g_frame_bufs[i]) return false;

        snprintf(path, sizeof(path), "%s/%s_%d.bin", dir, name, i);
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
    CPRINTF("Display: loaded %d frames for '%s' (%s)\n", count, name, dir);
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

// ---- Volume bar (left crescent, triangle) ----------------------------
// Right triangle: straight left edge at x=VB_X, apex at top, base at bottom.
// Width at each row grows linearly from 0 (apex) to VB_W (base).
// Fill grows from the base upward proportional to g_volume (0-100).

#define VB_X          12
#define VB_W          10
#define VB_TOP        68
#define VB_BOTTOM     172
#define VB_H          (VB_BOTTOM - VB_TOP)  // 104

#define VB_FILL_COLOR  0xFFFF   // white  — filled portion
#define VB_EMPTY_COLOR 0x3186   // dark gray — unfilled body

static int  s_vol_bar_prev  = -1;
static bool s_vol_bar_dirty = false;

static void drawVolumeBar() {
    if (!g_emoji_ready) return;
    int vol = g_volume;
    if (!s_vol_bar_dirty && vol == s_vol_bar_prev) return;
    s_vol_bar_prev  = vol;
    s_vol_bar_dirty = false;

    int16_t fill_top = (int16_t)(VB_BOTTOM - (int32_t)VB_H * vol / 100);

    for (int16_t y = VB_TOP; y <= VB_BOTTOM; y++) {
        int16_t w = (int16_t)((int32_t)VB_W * (y - VB_TOP) / VB_H);
        if (w <= 0) continue;
        uint16_t color = (y >= fill_top) ? VB_FILL_COLOR : VB_EMPTY_COLOR;
        gfx->drawFastHLine(VB_X, y, w, color);
    }
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

    // fillScreen wiped the volume bar — redraw it over the new frame
    s_vol_bar_dirty = true;
    drawVolumeBar();
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

    // Don't show emoji yet — boot status log displays first.
    CPRINTLN("Display: GC9A01 initialized");
}

// ---- Boot status log --------------------------------------------------
// Appends "> msg" lines on a black screen, accumulating as setup progresses.

static int  s_boot_log_y       = 60;
static bool s_boot_log_started = false;
static bool s_boot_log_locked  = false;   // true while a QR screen owns the display; suppresses appends

// Called when a QR screen is dismissed so the flag reflects current ownership.
void display_boot_log_unlock() {
    s_boot_log_locked = false;
}

void display_boot_status(const char* msg) {
    if (!g_emoji_ready) return;
    if (s_boot_log_locked) return;
    if (!s_boot_log_started) {
        gfx->fillScreen(RGB565_BLACK);
        s_boot_log_started = true;
    }
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(20, s_boot_log_y);
    gfx->print("> ");
    gfx->print(msg);
    s_boot_log_y += 10;
}

// ---- QR code renderer ------------------------------------------------
// Renders a QR centred horizontally at x_center, top edge at y_top.
//
// Uses the espressif esp_qrcode API bundled with arduino-esp32 (no external
// library). esp_qrcode_generate() takes a config + text and invokes
// display_func synchronously once encoding completes; the callback only
// receives the QR handle, so module_px / x_center / y_top are passed via
// file-scope statics. render_qr is only ever called from Core 1's loop()
// (display lives on Core 1) and esp_qrcode_generate is synchronous, so no
// re-entrancy concerns.

static int s_qr_module_px = 0;
static int s_qr_x_center  = 0;
static int s_qr_y_top     = 0;

static void render_qr_display_cb(esp_qrcode_handle_t qrcode) {
    int size = esp_qrcode_get_size(qrcode);
    int total_px = (size + 8) * s_qr_module_px;   // 4-module quiet zone each side
    int x0 = s_qr_x_center - total_px / 2;

    gfx->fillRect(x0, s_qr_y_top, total_px, total_px, RGB565_WHITE);

    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            if (esp_qrcode_get_module(qrcode, c, r)) {
                gfx->fillRect(
                    x0 + (c + 4) * s_qr_module_px,
                    s_qr_y_top + (r + 4) * s_qr_module_px,
                    s_qr_module_px, s_qr_module_px,
                    RGB565_BLACK
                );
            }
        }
    }
}

static void render_qr(const char* text, int version, int module_px, int x_center, int y_top) {
    s_qr_module_px = module_px;
    s_qr_x_center  = x_center;
    s_qr_y_top     = y_top;

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.display_func       = render_qr_display_cb;
    cfg.max_qrcode_version = version;
    cfg.qrcode_ecc_level   = ESP_QRCODE_ECC_LOW;
    esp_qrcode_generate(&cfg, text);
}

// ---- Network info screen ---------------------------------------------
// QR of http://IP (version 2, 5px/module = 165px), IP text, token count.

void display_info_screen() {
    if (!g_emoji_ready) return;
    s_boot_log_locked = true;   // QR screen owns the display now — no more boot log appends
    gfx->fillScreen(RGB565_BLACK);

    // QR: http://<IP>
    String ip = WiFi.localIP().toString();
    char url[32];
    snprintf(url, sizeof(url), "http://%s", ip.c_str());
    render_qr(url, 2, 5, 120, 12);
    int tx = (DISPLAY_WIDTH_PX - (int)ip.length() * 12) / 2;
    if (tx < 0) tx = 0;
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(tx, 185);
    gfx->print(ip);

    // Token count — textSize 1 = 6px per char
    char tok[24];
    snprintf(tok, sizeof(tok), "Tokens: %lu", (unsigned long)g_tokens.total);
    int tx2 = (DISPLAY_WIDTH_PX - (int)strlen(tok) * 6) / 2;
    if (tx2 < 0) tx2 = 0;
    gfx->setTextColor(0xC618);   // light gray
    gfx->setTextSize(1);
    gfx->setCursor(tx2, 207);
    gfx->print(tok);
}

// ---- Captive portal screen -------------------------------------------
// WiFi QR (version 3, 4px/module = 148px), SSID, join instructions.

void display_captive_portal_screen(const char* ssid) {
    if (!g_emoji_ready) return;
    s_boot_log_locked = true;   // QR screen owns the display now — no more boot log appends
    gfx->fillScreen(RGB565_BLACK);

    // WiFi QR — scanning auto-joins the AP; captive portal opens setup page
    char wifi_str[48];
    snprintf(wifi_str, sizeof(wifi_str), "WIFI:S:%s;T:nopass;;", ssid);
    render_qr(wifi_str, 3, 4, 120, 12);

    // SSID
    int tx = (DISPLAY_WIDTH_PX - (int)strlen(ssid) * 6) / 2;
    if (tx < 0) tx = 0;
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(tx, 167);
    gfx->print(ssid);

    // Instructions
    const char* line1 = "Scan to connect, then";
    int tx1 = (DISPLAY_WIDTH_PX - (int)strlen(line1) * 6) / 2;
    gfx->setCursor(tx1, 180);
    gfx->print(line1);

    const char* line2 = "open the setup page";
    int tx2 = (DISPLAY_WIDTH_PX - (int)strlen(line2) * 6) / 2;
    gfx->setCursor(tx2, 192);
    gfx->print(line2);
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

    // Animate: advance frame if enough time has passed and we have loaded frames.
    // count == 1 (PNG/JPG override) makes modulo always return 0 -> emoji stays static naturally.
    if (g_fs_ok && g_emoji_ready && g_loaded_emotion < EMOTION_COUNT) {
        uint8_t count = g_frame_counts[g_loaded_emotion];
        if (count > 0) {
            unsigned long now = millis();
            if (now - g_anim_last_ms >= EMOJI_FRAME_MS) {
                g_anim_last_ms = now;
                g_anim_frame = (g_anim_frame + 1) % count;
                emoji_draw_frame(g_anim_frame);
            }
        }
    }

    // Waveform — updated every loop iteration for smooth animation
    drawWaveform();

    // Volume bar — no-op unless g_volume changed
    drawVolumeBar();
}

#else

// Stub so loop() can call updateDisplay() unconditionally
static void updateDisplay() {}

#endif // USE_DISPLAY

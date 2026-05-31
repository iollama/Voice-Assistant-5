// =====================================================================
// NVS Config & WebServer Handlers
// =====================================================================

void load_agent_config() {
  preferences.begin("agentConfig", true);

  size_t blob_len = preferences.getBytesLength("sysPrompt");
  bool needs_sysprompt_migration = false;
  if (blob_len > 0) {
    char* buf = (char*)malloc(blob_len + 1);
    preferences.getBytes("sysPrompt", buf, blob_len);
    buf[blob_len] = '\0';
    g_sys_instruction = String(buf);
    free(buf);
  } else {
    String legacy = preferences.getString("sysPrompt", "");
    if (legacy.length() > 0) {
      g_sys_instruction = legacy;
      needs_sysprompt_migration = true;
    } else {
      g_sys_instruction = DEFAULT_SYS_INSTRUCTION;
    }
  }

  g_temperature = preferences.getFloat("temp", DEFAULT_TEMPERATURE);
  g_persist_conversation = preferences.getBool("persist", DEFAULT_PERSIST_CONVO);
  g_verbose_logging = preferences.getBool("verbose", DEFAULT_VERBOSE_LOGGING);
  g_api_key = preferences.getString("apiKey", "");
  g_volume = preferences.getInt("volume", 50);
  if (g_volume < 0) g_volume = 0;
  if (g_volume > 100) g_volume = 100;
  g_audio_output = preferences.getUChar("audioOut", AUDIO_OUT_SPEAKER);
  if (g_audio_output > AUDIO_OUT_HEADPHONES) g_audio_output = AUDIO_OUT_SPEAKER;
  g_voice = preferences.getString("voice", DEFAULT_VOICE);
  if (!voice_is_allowed(g_voice)) {
    CPRINTF("NVS: stored voice '%s' not in whitelist, falling back to '%s'\n",
            g_voice.c_str(), DEFAULT_VOICE);
    g_voice = DEFAULT_VOICE;
  }
  preferences.end();

  if (needs_sysprompt_migration) {
    preferences.begin("agentConfig", false);
    preferences.remove("sysPrompt");
    preferences.putBytes("sysPrompt", g_sys_instruction.c_str(),
                         g_sys_instruction.length());
    preferences.end();
    CPRINTF("NVS: migrated sysPrompt from string to blob (%u bytes)\n",
            (unsigned)g_sys_instruction.length());
  }
}

void handle_root() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Voice Agent - Config</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 20px; }
        .container { background-color: #fff; max-width: 600px; margin: auto; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h2, h3 { color: #333; }
        .form-group { margin-bottom: 15px; }
        .form-group-inline { display: flex; align-items: center; }
        .form-group-inline label { margin-right: 10px; font-weight: normal; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="password"], input[type="number"], textarea { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        textarea { resize: vertical; min-height: 220px; }
        .btn { background-color: #007bff; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; box-sizing: border-box; }
        .btn:hover { background-color: #0056b3; }
        .btn-danger { background-color: #dc3545; }
        .btn-danger:hover { background-color: #c82333; }
        .btn-secondary { background-color: #6c757d; }
        .btn-secondary:hover { background-color: #5a6268; }
        hr { border: 0; border-top: 1px solid #eee; margin: 20px 0; }
        .section { border: 1px solid #ddd; border-radius: 8px; padding: 20px; margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="container">
)rawliteral";

  html += "<h2>Voice Agent Settings</h2>";
  if (ap_mode) {
    html += "<p>First, connect this device to your WiFi network.</p>";
  } else {
    html += "<p>Device is connected to <b>" + WiFi.SSID() + "</b>.</p>";
  }

  html += R"rawliteral(
        <div class="section">
            <h3>WiFi Network</h3>
            <form action="/save" method="POST">
                <div class="form-group"><label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" required></div>
                <div class="form-group"><label for="pass">Password</label><input type="password" id="pass" name="pass"></div>
                <button type="submit" class="btn">Save & Restart</button>
            </form>
            <hr><form action="/delete" method="POST"><button type="submit" class="btn btn-danger">Forget Network</button></form>
        </div>
)rawliteral";

  html += "<div class='section'><h3>Agent Management</h3><form action='/saveAgent' method='POST'>";
  html += "<div class='form-group'><label for='sysPrompt'>System Prompt</label><textarea id='sysPrompt' name='sysPrompt' maxlength='12288' required>" + g_sys_instruction + "</textarea></div>";
  html += "<div class='form-group'><label for='voice'>Voice</label><select id='voice' name='voice'>";
  for (size_t i = 0; i < VOICE_OPTIONS_COUNT; ++i) {
    const char* sel = (g_voice == VOICE_OPTIONS[i].id) ? " selected" : "";
    html += "<option value='" + String(VOICE_OPTIONS[i].id) + "'" + sel + ">" + String(VOICE_OPTIONS[i].label) + "</option>";
  }
  html += "</select></div>";
  String persist_checked = g_persist_conversation ? "checked" : "";
  html += "<div class='form-group-inline'><input type='checkbox' id='persist' name='persist' value='true' " + persist_checked + "><label for='persist'>Persist Conversation</label></div>";
  String verbose_checked = g_verbose_logging ? "checked" : "";
  html += "<div class='form-group-inline'><input type='checkbox' id='verbose' name='verbose' value='true' " + verbose_checked + "><label for='verbose'>Verbose Logging</label></div>";
  html += "<br><button type='submit' class='btn'>Save Agent Settings</button></form><hr>";
  html += "<form action='/restoreAgent' method='POST' onsubmit=\"return confirm('Restore default agent settings?');\"><button type='submit' class='btn btn-secondary'>Restore Defaults</button></form></div>";

  html += "<div class='section'><h3>OpenAI API Key</h3><form action='/saveApiKey' method='POST'>";
  html += "<p>Status: <b>" + String(g_api_key.length() > 0 ? "Custom key is set" : "Using default key") + "</b></p>";
  html += "<div class='form-group'><label for='apiKey'>New API Key</label><input type='password' id='apiKey' name='apiKey' placeholder='sk-...' required></div>";
  html += "<button type='submit' class='btn'>Save API Key</button></form><hr>";
  html += "<form action='/clearApiKey' method='POST' onsubmit=\"return confirm('Clear saved key and revert to compiled default?');\"><button type='submit' class='btn btn-secondary'>Clear Saved Key</button></form></div>";

  html += "<div class='section'><h3>Volume</h3><form action='/saveVolume' method='POST'>";
  html += "<div class='form-group'><label for='volume'>Playback Volume: <span id='volVal'>" + String(g_volume) + "</span>%</label>";
  html += "<input type='range' id='volume' name='volume' min='0' max='100' value='" + String(g_volume) + "' oninput=\"document.getElementById('volVal').textContent=this.value\"></div>";
  html += "<button type='submit' class='btn'>Save Volume</button></form></div>";

  html += "<div class='section'><h3>Audio Output</h3><form action='/saveAudioOut' method='POST'>";
  html += "<div class='form-group'><label for='audioOut'>Output device</label>";
  html += "<select id='audioOut' name='audioOut'>";
  html += String("<option value='0'") + (g_audio_output == AUDIO_OUT_SPEAKER    ? " selected" : "") + ">Speaker</option>";
  html += String("<option value='1'") + (g_audio_output == AUDIO_OUT_HEADPHONES ? " selected" : "") + ">Headphones</option>";
  html += "</select></div>";
  html += "<p style='color:#666;font-size:0.9em;'>Headphones routes audio to the 3.5 mm jack, for an external speaker. Only use if you have an external speaker connected.</p>";
  html += "<button type='submit' class='btn'>Save Audio Output</button></form></div>";

#if USE_DISPLAY
  html += "<div class='section'><h3>Emojis</h3>";
  html += "<p>Customize the on-device emoji animations.</p>";
  html += "<a href='/emojis'><button class='btn'>Open Emoji Settings</button></a></div>";
#endif

  html += "<div class='section'><h3>Token Usage (since boot)</h3>";
  html += "<p style='color:#666;font-size:0.9em;'>Reload the page to refresh.</p>";
  html += "<div class='form-group'><label>Total</label>" + String((uint32_t)g_tokens.total) + "</div>";
  html += "<div class='form-group'><label>Input</label>";
  html += String((uint32_t)g_tokens.input);
  html += " (cached " + String((uint32_t)g_tokens.input_cached);
  html += ", text "  + String((uint32_t)g_tokens.input_text);
  html += ", audio " + String((uint32_t)g_tokens.input_audio) + ")</div>";
  html += "<div class='form-group'><label>Output</label>";
  html += String((uint32_t)g_tokens.output);
  html += " (text "  + String((uint32_t)g_tokens.output_text);
  html += ", audio " + String((uint32_t)g_tokens.output_audio) + ")</div>";
  html += "</div>";

  html += "</div></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handle_save() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (ssid.length() > 0) {
    preferences.begin("wifi-creds", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();
    server.send(200, "text/plain", "Credentials Saved! Restarting...");
    delay(2000); ESP.restart();
  }
}
void handle_delete() {
    preferences.begin("wifi-creds", false);
    preferences.clear();
    preferences.end();
    server.send(200, "text/plain", "Deleted! Restarting...");
    delay(2000); ESP.restart();
}
void handle_save_agent() {
  String submitted_prompt = server.arg("sysPrompt");
  if (submitted_prompt.length() > 12288) {
    server.send(413, "text/plain", "System prompt too large (max 12 KB)");
    return;
  }
  preferences.begin("agentConfig", false);
  g_sys_instruction = submitted_prompt;
  g_persist_conversation = server.hasArg("persist");
  g_verbose_logging = server.hasArg("verbose");
  preferences.putBytes("sysPrompt", g_sys_instruction.c_str(),
                       g_sys_instruction.length());
  preferences.putBool("persist", g_persist_conversation);
  preferences.putBool("verbose", g_verbose_logging);
  String submitted_voice = server.arg("voice");
  if (voice_is_allowed(submitted_voice)) {
    g_voice = submitted_voice;
    preferences.putString("voice", g_voice);
  } else if (submitted_voice.length() > 0) {
    CPRINTF("POST /saveAgent: voice '%s' not in whitelist, ignored\n",
            submitted_voice.c_str());
  }
  preferences.end();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_restore_agent() {
  preferences.begin("agentConfig", false);
  preferences.clear();
  preferences.end();
  load_agent_config();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_save_api_key() {
  String key = server.arg("apiKey");
  if (key.length() > 0) {
    preferences.begin("agentConfig", false);
    preferences.putString("apiKey", key);
    preferences.end();
    g_api_key = key;
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_clear_api_key() {
  preferences.begin("agentConfig", false);
  preferences.remove("apiKey");
  preferences.end();
  g_api_key = "";
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_save_volume() {
  g_volume = server.arg("volume").toInt();
  preferences.begin("agentConfig", false);
  preferences.putInt("volume", g_volume);
  preferences.end();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handle_save_audio_out() {
  // Reject mid-conversation — flipping mute pins while audio is flowing
  // would cause a pop and disrupt the turn. Same idle-gate pattern as
  // emoji uploads (emoji_in_turn).
  AssistantState s = (AssistantState)g_state;
  if (s == STATE_RECORDING || s == STATE_THINKING || s == STATE_SPEAKING) {
    server.send(409, "text/plain", "Busy - finish the current turn and try again.");
    return;
  }
  String v = server.arg("audioOut");
  uint8_t newOut = (v == "1" || v == "headphones") ? AUDIO_OUT_HEADPHONES : AUDIO_OUT_SPEAKER;
  if (newOut != g_audio_output) {
    g_audio_output = newOut;
    preferences.begin("agentConfig", false);
    preferences.putUChar("audioOut", g_audio_output);
    preferences.end();
    apply_audio_output();
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// =====================================================================
// EMOJI WEB API — see plans/so-this-is-jsut-velvety-puffin.md
// =====================================================================
#if USE_DISPLAY

static const char* EMOJI_CUSTOM_DIR  = "/custom";
static const char* EMOJI_STAGE_DIR   = "/custom_stage";

static int emoji_find_emotion_index(const String& name) {
  for (int e = 0; e < EMOTION_COUNT; e++) {
    if (name == EMOTION_NAMES[e]) return e;
  }
  return -1;
}

static bool emoji_in_turn() {
  AssistantState s = (AssistantState)g_state;
  return s == STATE_RECORDING || s == STATE_THINKING || s == STATE_SPEAKING;
}

// Resolve the directory currently serving frames for an emotion.
static const char* emoji_source_dir_for(uint8_t emotion) {
  char path[EMOJI_PATH_MAX];
  snprintf(path, sizeof(path), "/custom/%s_0.bin", EMOTION_NAMES[emotion]);
  return LittleFS.exists(path) ? "/custom" : "/default";
}

static void emoji_delete_custom_files(uint8_t emotion) {
  char path[EMOJI_PATH_MAX];
  for (int i = 0; i < EMOJI_NUM_FRAMES; i++) {
    snprintf(path, sizeof(path), "/custom/%s_%d.bin", EMOTION_NAMES[emotion], i);
    LittleFS.remove(path);
  }
}

static void emoji_cleanup_stage_for(uint8_t emotion) {
  char path[EMOJI_PATH_MAX];
  for (int i = 0; i < EMOJI_NUM_FRAMES; i++) {
    snprintf(path, sizeof(path), "/custom_stage/%s_%d.bin", EMOTION_NAMES[emotion], i);
    LittleFS.remove(path);
  }
}

// ---- GET /api/emoji/manifest ----------------------------------------
// Returns { "<emotion>": {"count": N, "source": "default"|"custom"}, ... }
void handle_emoji_manifest() {
  String json = "{";
  for (int e = 0; e < EMOTION_COUNT; e++) {
    if (e > 0) json += ",";
    const char* src = emoji_source_dir_for(e);
    const char* src_label = (strcmp(src, "/custom") == 0) ? "custom" : "default";
    json += "\""; json += EMOTION_NAMES[e]; json += "\":{";
    json += "\"count\":"; json += String((unsigned)g_frame_counts[e]);
    json += ",\"source\":\""; json += src_label; json += "\"}";
  }
  json += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// ---- GET /api/emoji/<emotion>/<n>.bin -------------------------------
void handle_emoji_get_frame(const String& uri) {
  int slash = uri.indexOf('/', 11);   // first slash after "/api/emoji/"
  if (slash < 0) { server.send(404, "text/plain", "bad path"); return; }
  String name = uri.substring(11, slash);
  int e = emoji_find_emotion_index(name);
  if (e < 0) { server.send(404, "text/plain", "unknown emotion"); return; }

  String tail = uri.substring(slash + 1);
  if (!tail.endsWith(".bin")) { server.send(404, "text/plain", "bad path"); return; }
  int idx = tail.substring(0, tail.length() - 4).toInt();
  if (idx < 0 || idx >= EMOJI_NUM_FRAMES) { server.send(404, "text/plain", "bad index"); return; }

  char path[EMOJI_PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s_%d.bin", emoji_source_dir_for((uint8_t)e), name.c_str(), idx);
  File f = LittleFS.open(path, "r");
  if (!f) { server.send(404, "text/plain", "not found"); return; }
  server.sendHeader("Cache-Control", "no-cache");
  server.streamFile(f, "application/octet-stream");
  f.close();
}

// ---- POST /api/emoji/<emotion>/reset --------------------------------
void handle_emoji_reset(uint8_t emotion) {
  if (emoji_in_turn()) {
    server.send(409, "text/plain", "Busy - try again after the current response.");
    return;
  }
  emoji_delete_custom_files(emotion);
  emoji_rescan_frame_counts();
  g_loaded_emotion = 0xFF;
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---- POST /api/emoji/reset-all --------------------------------------
void handle_emoji_reset_all() {
  if (emoji_in_turn()) {
    server.send(409, "text/plain", "Busy - try again after the current response.");
    return;
  }
  for (int e = 0; e < EMOTION_COUNT; e++) emoji_delete_custom_files((uint8_t)e);
  emoji_rescan_frame_counts();
  g_loaded_emotion = 0xFF;
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---- POST /api/emoji/<emotion> --------------------------------------
// Multipart with single file field. Body bytes are N x EMOJI_FRAME_BYTES, N in 1..8.
// Slices on-the-fly into /custom_stage/<emotion>_<i>.bin during streaming.
struct EmojiUploadState {
  int   emotion;
  File  f;
  int   frame_idx;
  size_t frame_bytes;
  size_t total_bytes;
  bool  failed;
  int   http_status;
  String error_msg;
};
static EmojiUploadState g_eu = {-1, File(), 0, 0, 0, false, 200, ""};

static void eu_fail(int status, const char* msg) {
  if (g_eu.failed) return;
  g_eu.failed = true;
  g_eu.http_status = status;
  g_eu.error_msg = msg;
}

void handle_emoji_replace_chunk() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    // Reset state for the new multipart file
    g_eu.frame_idx = 0;
    g_eu.frame_bytes = 0;
    g_eu.total_bytes = 0;
    g_eu.failed = false;
    g_eu.http_status = 200;
    g_eu.error_msg = "";
    if (g_eu.f) g_eu.f.close();

    if (g_emoji_upload_active) {
      eu_fail(409, "Another upload in progress");
      return;
    }
    g_emoji_upload_active = true;

    if (emoji_in_turn()) {
      eu_fail(409, "Busy - try again after the current response.");
      return;
    }

    String uri = server.uri();
    if (!uri.startsWith("/api/emoji/")) { eu_fail(404, "Bad path"); return; }
    String name = uri.substring(11);
    int e = emoji_find_emotion_index(name);
    if (e < 0) { eu_fail(404, "Unknown emotion"); return; }
    g_eu.emotion = e;

    size_t free_bytes = LittleFS.totalBytes() - LittleFS.usedBytes();
    if (free_bytes < (size_t)EMOJI_FRAME_BYTES * EMOJI_NUM_FRAMES + 4096) {
      eu_fail(507, "Insufficient storage");
      return;
    }

    LittleFS.mkdir(EMOJI_STAGE_DIR);

    char path[EMOJI_PATH_MAX];
    snprintf(path, sizeof(path), "/custom_stage/%s_0.bin", EMOTION_NAMES[e]);
    g_eu.f = LittleFS.open(path, "w");
    if (!g_eu.f) { eu_fail(500, "Open staging failed"); return; }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (g_eu.failed) return;
    const uint8_t* data = upload.buf;
    size_t remaining = upload.currentSize;
    while (remaining > 0) {
      if (g_eu.frame_bytes == (size_t)EMOJI_FRAME_BYTES) {
        g_eu.f.close();
        g_eu.frame_idx++;
        g_eu.frame_bytes = 0;
        if (g_eu.frame_idx >= EMOJI_NUM_FRAMES) { eu_fail(400, "Too many frames"); return; }
        char path[EMOJI_PATH_MAX];
        snprintf(path, sizeof(path), "/custom_stage/%s_%d.bin",
                 EMOTION_NAMES[g_eu.emotion], g_eu.frame_idx);
        g_eu.f = LittleFS.open(path, "w");
        if (!g_eu.f) { eu_fail(500, "Open staging failed"); return; }
      }
      size_t room = (size_t)EMOJI_FRAME_BYTES - g_eu.frame_bytes;
      size_t to_write = remaining < room ? remaining : room;
      size_t wrote = g_eu.f.write(data, to_write);
      if (wrote != to_write) { eu_fail(507, "Write failed"); return; }
      g_eu.frame_bytes += wrote;
      g_eu.total_bytes += wrote;
      data += wrote;
      remaining -= wrote;
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_eu.f) g_eu.f.close();

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (g_eu.f) g_eu.f.close();
    eu_fail(400, "Upload aborted");
  }
}

void handle_emoji_replace_done() {
  if (!g_emoji_upload_active || g_eu.emotion < 0) {
    server.send(400, "text/plain", "No upload data");
    return;
  }

  if (g_eu.failed) {
    if (g_eu.emotion >= 0) emoji_cleanup_stage_for((uint8_t)g_eu.emotion);
    server.send(g_eu.http_status,
                "text/plain",
                g_eu.error_msg.length() ? g_eu.error_msg : String("Upload failed"));
    g_emoji_upload_active = false;
    return;
  }

  if (g_eu.total_bytes == 0 || (g_eu.total_bytes % EMOJI_FRAME_BYTES) != 0) {
    emoji_cleanup_stage_for((uint8_t)g_eu.emotion);
    server.send(400, "text/plain", "Upload size must be a multiple of 45000 bytes");
    g_emoji_upload_active = false;
    return;
  }
  int new_count = g_eu.total_bytes / EMOJI_FRAME_BYTES;
  if (new_count < 1 || new_count > EMOJI_NUM_FRAMES) {
    emoji_cleanup_stage_for((uint8_t)g_eu.emotion);
    server.send(400, "text/plain", "Bad frame count");
    g_emoji_upload_active = false;
    return;
  }

  LittleFS.mkdir(EMOJI_CUSTOM_DIR);
  emoji_delete_custom_files((uint8_t)g_eu.emotion);

  char src[EMOJI_PATH_MAX], dst[EMOJI_PATH_MAX];
  const char* name = EMOTION_NAMES[g_eu.emotion];
  for (int i = 0; i < new_count; i++) {
    snprintf(src, sizeof(src), "/custom_stage/%s_%d.bin", name, i);
    snprintf(dst, sizeof(dst), "/custom/%s_%d.bin",       name, i);
    if (!LittleFS.rename(src, dst)) {
      CPRINTF("Emoji commit: rename %s -> %s failed\n", src, dst);
    }
  }
  // remove any leftover staging files for higher indices (shouldn't exist but be safe)
  for (int i = new_count; i < EMOJI_NUM_FRAMES; i++) {
    snprintf(src, sizeof(src), "/custom_stage/%s_%d.bin", name, i);
    LittleFS.remove(src);
  }

  g_frame_counts[g_eu.emotion] = (uint8_t)new_count;
  g_loaded_emotion = 0xFF;

  CPRINTF("Emoji replaced: %s (%d frames)\n", name, new_count);
  String resp = "{\"ok\":true,\"emotion\":\"";
  resp += name;
  resp += "\",\"count\":";
  resp += String(new_count);
  resp += "}";
  server.send(200, "application/json", resp);
  g_emoji_upload_active = false;
}

// ---- GET /emojis  ---------------------------------------------------
// Page HTML+JS lives in emojis_page.h to bypass the Arduino auto-prototyper,
// which mis-parses `async function` inside the raw string as C++ prototypes.
#include "emojis_page.h"
void handle_emojis_page() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", EMOJIS_PAGE_HTML);
}

#endif // USE_DISPLAY

// =====================================================================
// catch-all dispatcher (also handles emoji API)
// =====================================================================
void handle_not_found() {
#if USE_DISPLAY
  String uri = server.uri();
  if (uri.startsWith("/api/emoji/")) {
    if (server.method() == HTTP_GET) {
      if (uri == "/api/emoji/manifest") { handle_emoji_manifest(); return; }
      handle_emoji_get_frame(uri); return;
    } else if (server.method() == HTTP_POST) {
      if (uri == "/api/emoji/reset-all") { handle_emoji_reset_all(); return; }
      if (uri.endsWith("/reset")) {
        String name = uri.substring(11, uri.length() - 6);
        int e = emoji_find_emotion_index(name);
        if (e >= 0) { handle_emoji_reset((uint8_t)e); return; }
      }
    }
    server.send(404, "text/plain", "emoji api: not found");
    return;
  }
#endif
  if (ap_mode) {
    server.send(302, "text/plain", "http://" + WiFi.softAPIP().toString());
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

void setup_web_server() {
#if USE_DISPLAY
  display_boot_status("Opening the front door");
#endif
  server.on("/", HTTP_GET, handle_root);
  server.on("/save", HTTP_POST, handle_save);
  server.on("/delete", HTTP_POST, handle_delete);
  server.on("/saveAgent", HTTP_POST, handle_save_agent);
  server.on("/restoreAgent", HTTP_POST, handle_restore_agent);
  server.on("/saveApiKey", HTTP_POST, handle_save_api_key);
  server.on("/clearApiKey", HTTP_POST, handle_clear_api_key);
  server.on("/saveVolume", HTTP_POST, handle_save_volume);
  server.on("/saveAudioOut", HTTP_POST, handle_save_audio_out);

#if USE_DISPLAY
  server.on("/emojis", HTTP_GET, handle_emojis_page);
  // Register POST /api/emoji/<emotion> for each emotion (needs upload callback)
  for (int e = 0; e < EMOTION_COUNT; e++) {
    String path = String("/api/emoji/") + EMOTION_NAMES[e];
    server.on(path, HTTP_POST, handle_emoji_replace_done, handle_emoji_replace_chunk);
  }
#endif

  server.onNotFound(handle_not_found);
  server.begin();
}

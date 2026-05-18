// =====================================================================
// TOOL REGISTRY (CORE 0) — OpenAI Realtime function tools
//
// To add a new tool:
//   1. Write a static handler: static void handleXxxToolCall(const String& payload)
//      Call beginToolCall(call_id) first, then do your work, then set g_tool_result_output.
//   2. Add a Tool entry to TOOLS[] below (name, description, parameters JSON, handler).
//   buildToolsJson() and dispatchToolCall() pick it up automatically — no other files change.
// =====================================================================

// ---- Common handler boilerplate ----------------------------------------

static void beginToolCall(const char* call_id) {
    g_pending_call_id        = String(call_id);
    g_awaiting_tool_response = true;
    g_tool_result_output     = "ok";
}

// ---- Tool handlers -----------------------------------------------------

static void handleEmotionToolCall(const String& payload) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        const char* args_str = doc["arguments"];
        beginToolCall(doc["call_id"] | "");
        if (args_str) {
            DynamicJsonDocument args(256);
            if (deserializeJson(args, args_str) == DeserializationError::Ok) {
                const char* emotion   = args["emotion"]   | "neutral";
                float       intensity = args["intensity"] | 1.0f;
                CPRINTF("MOOD: %s (intensity: %.1f)\n", emotion, intensity);
                for (uint8_t e = 0; e < EMOTION_COUNT; e++) {
                    if (strcmp(emotion, EMOTION_NAMES[e]) == 0) {
                        g_current_emotion = e;
                        break;
                    }
                }
            }
        }
    }
}

static void handleVolumeToolCall(const String& payload) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return;

    const char* args_str = doc["arguments"];
    beginToolCall(doc["call_id"] | "");

    if (!args_str) return;
    DynamicJsonDocument args(256);
    if (deserializeJson(args, args_str) != DeserializationError::Ok) return;

    int new_vol = g_volume;
    if (args.containsKey("volume")) {
        new_vol = args["volume"].as<int>();
    } else if (args.containsKey("delta")) {
        new_vol = (int)g_volume + args["delta"].as<int>();
    }
    if (new_vol < 0)   new_vol = 0;
    if (new_vol > 100) new_vol = 100;

    g_volume = new_vol;
    g_volume_persist_pending = true;  // Core 1's loop() writes NVS
    CPRINTF("VOLUME: set to %d\n", new_vol);

    g_tool_result_output = String("volume set to ") + new_vol;
}

static void handleNetworkInfoToolCall(const String& payload) {
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return;
    beginToolCall(doc["call_id"] | "");
    g_show_info_screen = true;
}

// ---- Tool registry -----------------------------------------------------

static const Tool TOOLS[] = {
    {
        "set_display_emotion",
        "Set the display animation state to match the emotional tone of your response. "
        "Call this at the start of each response.",
        "{\"type\":\"object\",\"properties\":{"
        "\"emotion\":{\"type\":\"string\",\"enum\":[\"neutral\",\"happy\",\"excited\","
        "\"empathetic\",\"confused\",\"concerned\",\"thinking\"]},"
        "\"intensity\":{\"type\":\"number\",\"minimum\":0.0,\"maximum\":1.0}"
        "},\"required\":[\"emotion\"]}",
        handleEmotionToolCall
    },
    {
        "set_volume",
        "Adjust the speaker playback volume. Pass 'volume' for an absolute level (0-100, "
        "where 50 is normal) or 'delta' for a relative change (e.g. +15 louder, -20 quieter). "
        "The change persists across reboots.",
        "{\"type\":\"object\",\"properties\":{"
        "\"volume\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100,"
        "\"description\":\"Absolute target volume.\"},"
        "\"delta\":{\"type\":\"integer\",\"minimum\":-100,\"maximum\":100,"
        "\"description\":\"Relative change to current volume.\"}}}",
        handleVolumeToolCall
    },
    {
        "show_network_info",
        "Display the device IP address and session token usage on the screen. Use when the user "
        "asks about network info, IP address, or how to access the web UI. "
        "The display stays until the user speaks again.",
        "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        handleNetworkInfoToolCall
    }
};
static const int TOOL_COUNT = sizeof(TOOLS) / sizeof(TOOLS[0]);

// ---- Public helpers (called from protocol.ino) -------------------------

// Appends the tools JSON array to `out`. Called once per session at connect time.
void buildToolsJson(String& out) {
    out += "[";
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (i > 0) out += ",";
        out += "{\"type\":\"function\",\"name\":\"";
        out += TOOLS[i].name;
        out += "\",\"description\":";
        out += jsonEscape(String(TOOLS[i].description));
        out += ",\"parameters\":";
        out += TOOLS[i].parameters_json;
        out += "}";
    }
    out += "]";
}

// Finds the matching tool by name and calls its handler.
void dispatchToolCall(const String& payload) {
    for (int i = 0; i < TOOL_COUNT; i++) {
        if (payload.indexOf(TOOLS[i].name) > 0) {
            CPRINTF("WS: Tool call: %s\n", TOOLS[i].name);
            TOOLS[i].handler(payload);
            return;
        }
    }
    CPRINTLN("WS: Unknown tool call received");
}

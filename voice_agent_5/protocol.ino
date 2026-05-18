// =====================================================================
// PROTOCOL TASK (CORE 0) — OpenAI Realtime WebSockets
// =====================================================================
using namespace websockets;

// ---- Helpers ---------------------------------------------------------

String get_api_key() {
  return (g_api_key.length() > 0) ? g_api_key : String(OPENAI_API_KEY);
}

String jsonEscape(const String& s) {
  String out = "\"";
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          sprintf(buf, "\\u%04x", (uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += "\"";
  return out;
}

// Encode audio chunk into pre-allocated static buffers and send. No heap allocation.
static void sendAudioChunk(websockets::WebsocketsClient& ws, const uint8_t* data, size_t len) {
    size_t b64_len = 0;
    mbedtls_base64_encode((unsigned char*)s_b64_buf, B64_CHUNK_SIZE, &b64_len, data, len);
    s_b64_buf[b64_len] = '\0';
    snprintf(s_ws_frame, WS_FRAME_SIZE,
             "{\"type\":\"input_audio_buffer.append\",\"audio\":\"%s\"}", s_b64_buf);
    ws.send(s_ws_frame);
}

// ---- Message handlers ------------------------------------------------

// Fast manual parse — called on every audio frame, latency critical
static void handleAudioDelta(const String& payload) {
    int audioStart = payload.indexOf("\"delta\":\"") + 9;
    int audioEnd   = (audioStart > 8) ? payload.indexOf("\"", audioStart) : -1;
    if (audioStart > 8 && audioEnd > audioStart) {
        String b64 = payload.substring(audioStart, audioEnd);
        size_t olen = 0;
        mbedtls_base64_decode(NULL, 0, &olen, (const unsigned char*)b64.c_str(), b64.length());
        if (olen > 0 && olen <= AUDIO_DECODE_SIZE && s_decode_buf) {
            mbedtls_base64_decode(s_decode_buf, AUDIO_DECODE_SIZE, &olen,
                                  (const unsigned char*)b64.c_str(), b64.length());
            out_ring_buf->write(s_decode_buf, olen);
            VPRINTF("WS: Decoded and buffered %d bytes of audio\n", olen);
        }
    }
}

static void handleResponseDone(const String& payload) {
    // Only the "response.done" event carries usage; "response.audio.done" does not.
    if (payload.indexOf("\"response.done\"") > 0) {
        DynamicJsonDocument doc(2048);
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
            JsonObject usage = doc["response"]["usage"];
            uint32_t in_total  = usage["input_tokens"]  | 0;
            uint32_t out_total = usage["output_tokens"] | 0;
            uint32_t tot       = usage["total_tokens"]  | 0;
            g_tokens.total        += tot;
            g_tokens.input        += in_total;
            g_tokens.output       += out_total;
            g_tokens.input_cached += (uint32_t)(usage["input_token_details"]["cached_tokens"] | 0);
            g_tokens.input_text   += (uint32_t)(usage["input_token_details"]["text_tokens"]   | 0);
            g_tokens.input_audio  += (uint32_t)(usage["input_token_details"]["audio_tokens"]  | 0);
            g_tokens.output_text  += (uint32_t)(usage["output_token_details"]["text_tokens"]  | 0);
            g_tokens.output_audio += (uint32_t)(usage["output_token_details"]["audio_tokens"] | 0);
            VPRINTF("WS: usage +%u in / +%u out (boot total %u)\n",
                    (unsigned)in_total, (unsigned)out_total, (unsigned)g_tokens.total);
        }
    }

    if (g_awaiting_tool_response) {
        g_awaiting_tool_response = false;
        g_tool_result_ready      = true;
    } else {
        CPRINTLN("WS: response done");
        ws_response_done = true;
    }
}

// ---- Session setup ---------------------------------------------------

static String buildSessionUpdate() {
    String s = "{\"type\":\"session.update\",\"session\":{"
               "\"type\":\"realtime\","
               "\"instructions\":";
    s += jsonEscape(g_sys_instruction);
    s += ",\"audio\":{"
           "\"input\":{\"format\":{\"type\":\"audio/pcm\",\"rate\":24000},\"turn_detection\":null},"
           "\"output\":{\"format\":{\"type\":\"audio/pcm\",\"rate\":24000},\"voice\":";
    s += jsonEscape(g_voice);
    s += "}},"
         "\"reasoning\":{\"effort\":\"low\"},"
         "\"tools\":";
    buildToolsJson(s);
    s += ",\"tool_choice\":\"auto\"}}";
    return s;
}

// ---- Poll-loop helpers -----------------------------------------------

static void submitToolResult(websockets::WebsocketsClient& ws) {
    if (!g_tool_result_ready) return;
    g_tool_result_ready = false;
    String funcResult = "{\"type\":\"conversation.item.create\",\"item\":"
                        "{\"type\":\"function_call_output\",\"call_id\":\"";
    funcResult += g_pending_call_id;
    funcResult += "\",\"output\":";
    funcResult += jsonEscape(g_tool_result_output);
    funcResult += "}}";
    ws.send(funcResult);
    ws.send("{\"type\":\"response.create\"}");
    CPRINTLN("WS: Tool handled, requesting audio response");
}

static void handleBargein(websockets::WebsocketsClient& ws) {
    if (!cmd_cancel) return;
    cmd_cancel = false;
    CPRINTLN("WS: Sending response.cancel (Barge-in)");
    ws.send("{\"type\":\"response.cancel\"}");
    ws_response_done = false;
}

static void streamMicAudio(websockets::WebsocketsClient& ws) {
    if (g_state != STATE_RECORDING) return;
    if (in_ring_buf->available() >= AUDIO_CHUNK_SIZE) {
        in_ring_buf->read(s_mic_chunk, AUDIO_CHUNK_SIZE);
        sendAudioChunk(ws, s_mic_chunk, AUDIO_CHUNK_SIZE);
    }
}

static void handleCommit(websockets::WebsocketsClient& ws) {
    if (!cmd_commit) return;
    cmd_commit = false;
    // Flush remaining mic buffer in AUDIO_CHUNK_SIZE chunks — no malloc needed
    while (in_ring_buf->available() > 0) {
        size_t to_read = min((size_t)AUDIO_CHUNK_SIZE, in_ring_buf->available());
        in_ring_buf->read(s_mic_chunk, to_read);
        sendAudioChunk(ws, s_mic_chunk, to_read);
    }
    CPRINTLN("WS: Sending input_audio_buffer.commit");
    ws.send("{\"type\":\"input_audio_buffer.commit\"}");
    CPRINTLN("WS: Sending response.create");
    ws.send("{\"type\":\"response.create\"}");
}

// ---- Main protocol loop ----------------------------------------------

void manage_websockets() {
    WebsocketsClient wsClient;
    wsClient.setInsecure();

    wsClient.onMessage([](WebsocketsMessage message) {
        String payload = message.data();
        if (payload.indexOf("\"response.output_audio.delta\"") > 0) {
            handleAudioDelta(payload);
        } else if (payload.indexOf("\"response.function_call_arguments.done\"") > 0) {
            dispatchToolCall(payload);
        } else if (payload.indexOf("\"response.done\"") > 0 || payload.indexOf("\"response.output_audio.done\"") > 0) {
            handleResponseDone(payload);
        } else if (payload.indexOf("\"error\"") > 0) {
            CPRINTLN("WS Error:");
            CPRINTLN(payload);
        } else {
            if (payload.length() < 500) {
                VPRINTLN("WS Event: " + payload);
            } else {
                VPRINTLN("WS Event: [Large Payload]");
            }
        }
    });

    wsClient.onEvent([](WebsocketsEvent event, String data) {
        if (event == WebsocketsEvent::ConnectionOpened) {
            CPRINTLN("WS Event: Connection Opened");
        } else if (event == WebsocketsEvent::ConnectionClosed) {
            CPRINT("WS Event: Connection Closed. Reason: ");
            CPRINTLN(data);
        } else if (event == WebsocketsEvent::GotPing) {
            VPRINTLN("WS Event: Got Ping");
        } else if (event == WebsocketsEvent::GotPong) {
            VPRINTLN("WS Event: Got Pong");
        }
    });

    CPRINTF("Connecting to OpenAI Realtime API... Free Heap: %d\n", ESP.getFreeHeap());
    String key = get_api_key();
    key.trim(); // Ensure no hidden newlines break the HTTP headers
    if (key.length() < 10) {
        CPRINTLN("WARNING: API Key seems invalid or empty!");
    }

    wsClient.addHeader("Authorization", "Bearer " + key);

    if (!wsClient.connectSecure("api.openai.com", 443, "/v1/realtime?model=gpt-realtime-2")) {
        CPRINTLN("WS Connection failed!");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }
    CPRINTLN("WS Connected!");
    setAssistantState(STATE_READY_FOR_INPUT);

    CPRINTLN("WS: Sending session.update...");
    CPRINTF("WS: System prompt: %s\n", g_sys_instruction.c_str());
    wsClient.send(buildSessionUpdate());

    while (wsClient.available() && WiFi.status() == WL_CONNECTED) {
        wsClient.poll();
        submitToolResult(wsClient);
        handleBargein(wsClient);
        streamMicAudio(wsClient);
        handleCommit(wsClient);
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    CPRINTLN("WS Disconnected.");
    setAssistantState(STATE_WIFI_WAIT);
}

void protocol_task(void* pvParameters) {
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    while (true) {
        if (WiFi.status() == WL_CONNECTED && !ap_mode) {
            manage_websockets();
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

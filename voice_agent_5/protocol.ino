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

// Infrequent tool call — latency not critical, use full JSON parse
static void handleEmotionToolCall(const String& payload) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        const char* args_str = doc["arguments"];
        const char* call_id  = doc["call_id"] | "";
        g_pending_call_id        = String(call_id);
        g_awaiting_tool_response = true;
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

static void handleResponseDone() {
    if (g_awaiting_tool_response) {
        g_awaiting_tool_response = false;
        g_tool_result_ready      = true;
    } else {
        CPRINTLN("WS: response done");
        ws_response_done = true;
    }
}

// ---- Main protocol loop ----------------------------------------------

void manage_websockets() {
    WebsocketsClient wsClient;
    wsClient.setInsecure();

    wsClient.onMessage([](WebsocketsMessage message) {
        String payload = message.data();
        if (payload.indexOf("\"response.audio.delta\"") > 0) {
            handleAudioDelta(payload);
        } else if (payload.indexOf("\"response.function_call_arguments.done\"") > 0
                   && payload.indexOf("\"set_display_emotion\"") > 0) {
            handleEmotionToolCall(payload);
        } else if (payload.indexOf("\"response.done\"") > 0 || payload.indexOf("\"response.audio.done\"") > 0) {
            handleResponseDone();
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
    wsClient.addHeader("OpenAI-Beta", "realtime=v1");

    if (!wsClient.connectSecure("api.openai.com", 443, "/v1/realtime?model=gpt-realtime-1.5")) {
        CPRINTLN("WS Connection failed!");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }
    CPRINTLN("WS Connected!");
    setAssistantState(STATE_READY_FOR_INPUT);

    String sessionUpdate = "{\"type\":\"session.update\",\"session\":{\"modalities\":[\"audio\",\"text\"],\"instructions\":";
    sessionUpdate += jsonEscape(g_sys_instruction);
    sessionUpdate += ",\"voice\":\"alloy\",\"input_audio_format\":\"pcm16\",\"output_audio_format\":\"pcm16\",\"turn_detection\":null,\"temperature\":";
    sessionUpdate += String(g_temperature);
    sessionUpdate += ",\"tools\":[{\"type\":\"function\",\"name\":\"set_display_emotion\","
                     "\"description\":\"Set the display animation state to match the emotional tone of your response. Call this at the start of each response.\","
                     "\"parameters\":{\"type\":\"object\",\"properties\":{"
                     "\"emotion\":{\"type\":\"string\",\"enum\":[\"neutral\",\"happy\",\"excited\",\"empathetic\",\"confused\",\"concerned\",\"thinking\"]},"
                     "\"intensity\":{\"type\":\"number\",\"minimum\":0.0,\"maximum\":1.0}"
                     "},\"required\":[\"emotion\"]}}],\"tool_choice\":\"auto\""
                     "}}";
    CPRINTLN("WS: Sending session.update...");
    CPRINTF("WS: System prompt: %s\n", g_sys_instruction.c_str());
    wsClient.send(sessionUpdate);

    while (wsClient.available() && WiFi.status() == WL_CONNECTED) {
        wsClient.poll();

        if (g_tool_result_ready) {
            g_tool_result_ready = false;
            String funcResult = "{\"type\":\"conversation.item.create\",\"item\":"
                                "{\"type\":\"function_call_output\",\"call_id\":\"";
            funcResult += g_pending_call_id;
            funcResult += "\",\"output\":\"ok\"}}";
            wsClient.send(funcResult);
            wsClient.send("{\"type\":\"response.create\"}");
            CPRINTLN("WS: Emotion tool handled, requesting audio response");
        }

        if (cmd_cancel) {
            cmd_cancel = false;
            CPRINTLN("WS: Sending response.cancel (Barge-in)");
            wsClient.send("{\"type\":\"response.cancel\"}");
            ws_response_done = false;
        }

        if (g_state == STATE_RECORDING) {
            if (in_ring_buf->available() >= AUDIO_CHUNK_SIZE) {
                in_ring_buf->read(s_mic_chunk, AUDIO_CHUNK_SIZE);
                sendAudioChunk(wsClient, s_mic_chunk, AUDIO_CHUNK_SIZE);
            }
        }

        if (cmd_commit) {
            cmd_commit = false;
            // Flush remaining mic buffer in AUDIO_CHUNK_SIZE chunks — no malloc needed
            while (in_ring_buf->available() > 0) {
                size_t to_read = min((size_t)AUDIO_CHUNK_SIZE, in_ring_buf->available());
                in_ring_buf->read(s_mic_chunk, to_read);
                sendAudioChunk(wsClient, s_mic_chunk, to_read);
            }
            CPRINTLN("WS: Sending input_audio_buffer.commit");
            wsClient.send("{\"type\":\"input_audio_buffer.commit\"}");
            CPRINTLN("WS: Sending response.create");
            wsClient.send("{\"type\":\"response.create\"}");
        }

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

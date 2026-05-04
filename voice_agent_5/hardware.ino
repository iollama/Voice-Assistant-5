// =====================================================================
// HARDWARE — I2S Setup, Microphone, Speaker, Button
// (Core 1 helpers called from loop())
// =====================================================================

// ---- I2S Setup -------------------------------------------------------

void setup_i2s_mic() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE_IN,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 is 32-bit/24-bit
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK_IN,
    .ws_io_num = I2S_WS_IN,
    .data_out_num = -1,
    .data_in_num = I2S_SD_IN
  };
  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
  CPRINTLN("I2S Mic Ready");
}

void setup_i2s_speaker() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE_OUT,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK_OUT,
    .ws_io_num = I2S_LRCLK_OUT,
    .data_out_num = I2S_DOUT_OUT,
    .data_in_num = -1
  };
  i2s_driver_install(I2S_NUM_1, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pins);
  i2s_zero_dma_buffer(I2S_NUM_1);
  CPRINTLN("I2S Speaker Ready");
}

// ---- Shared silence helper -------------------------------------------

// Write one DMA_BLOCK_SIZE chunk of silence to the speaker I2S bus.
// Pass block=true to wait for DMA (portMAX_DELAY), false for non-blocking.
static void writeSilence(bool block) {
    static const uint8_t zero[DMA_BLOCK_SIZE] = {0};
    size_t bw = 0;
    i2s_write(I2S_NUM_1, zero, DMA_BLOCK_SIZE, &bw, block ? portMAX_DELAY : 0);
}

// ---- PTT Button ------------------------------------------------------

static bool s_buttonState       = false;
static bool s_lastButtonReading = false;
static unsigned long s_lastDebounceTime = 0;

static void updateButtonState() {
    bool reading = (digitalRead(BUTTON_PIN) == LOW);
    unsigned long now = millis();
    if (reading != s_lastButtonReading) {
        s_lastDebounceTime = now;
    }
    if ((now - s_lastDebounceTime) > 50) {
        if (reading != s_buttonState) {
            s_buttonState = reading;
            if (s_buttonState) {
                CPRINTLN("PTT PRESSED");
                out_ring_buf->clear();
                in_ring_buf->clear();
                cmd_cancel = true;
                setAssistantState(STATE_RECORDING);
            } else {
                CPRINTLN("PTT RELEASED");
                g_audio_level = 0;
                cmd_commit = true;
                setAssistantState(STATE_THINKING);
            }
        }
    }
    s_lastButtonReading = reading;
}

// ---- Microphone Capture ----------------------------------------------

static void captureMicrophone() {
    size_t bytes_read = 0;
    uint8_t mic_buf[DMA_BLOCK_SIZE];
    i2s_read(I2S_NUM_0, mic_buf, DMA_BLOCK_SIZE, &bytes_read, 0);
    if (g_state == STATE_RECORDING && bytes_read > 0) {
        int samples = bytes_read / 4;
        int32_t* raw32 = (int32_t*)mic_buf;
        int16_t* pcm16 = (int16_t*)mic_buf;
        int16_t peak = 0;
        for (int i = 0; i < samples; i++) {
            pcm16[i] = (int16_t)(raw32[i] >> 16);
            int16_t abs_val = (pcm16[i] < 0) ? -pcm16[i] : pcm16[i];
            if (abs_val > peak) peak = abs_val;
        }
        in_ring_buf->write((uint8_t*)pcm16, samples * 2);
        // Map 16-bit peak to 0–255 for display (log-ish scale)
        uint8_t level = (peak > 32) ? (uint8_t)min(255, (int)(log2f((float)peak) * 16.0f)) : 0;
        g_audio_level = level;
    }
    // Always drain DMA even when not recording to prevent stale audio
}

// ---- Speaker Playback & Jitter Management ----------------------------

static void manageSpeaker() {
    size_t avail = out_ring_buf->available();

    // Start speaking once jitter threshold is met, or response is done and data is waiting
    if (g_state != STATE_SPEAKING && (avail >= JITTER_BUFFER_BYTES || (ws_response_done && avail > 0))) {
        CPRINTF("Speaker: Starting playback (%d bytes, response_done=%d)\n", avail, ws_response_done);
        setAssistantState(STATE_SPEAKING);
    }

    if (g_state == STATE_SPEAKING) {
        if (avail < LOW_WATER_BYTES && !ws_response_done) {
            // Low water — rebuffer to avoid choppy underrun gaps mid-stream
            CPRINTF("Speaker: Rebuffering (only %d bytes left)\n", avail);
            setAssistantState(STATE_THINKING);
            writeSilence(false);
        } else if (avail > 0) {
            size_t to_read = (avail > DMA_BLOCK_SIZE) ? DMA_BLOCK_SIZE : avail;
            uint8_t spk_buf[DMA_BLOCK_SIZE];
            out_ring_buf->read(spk_buf, to_read);

            if (g_volume < 100) {
                int16_t* samples = (int16_t*)spk_buf;
                for (size_t i = 0; i < to_read / 2; i++) {
                    samples[i] = (int16_t)(((int32_t)samples[i] * g_volume) / 100);
                }
            }
            // Compute peak for sound bar
            {
                int16_t* samples = (int16_t*)spk_buf;
                int16_t peak = 0;
                for (size_t i = 0; i < to_read / 2; i++) {
                    int16_t abs_val = (samples[i] < 0) ? -samples[i] : samples[i];
                    if (abs_val > peak) peak = abs_val;
                }
                uint8_t level = (peak > 32) ? (uint8_t)min(255, (int)(log2f((float)peak) * 16.0f)) : 0;
                g_audio_level = level;
            }
            size_t bw = 0;
            i2s_write(I2S_NUM_1, spk_buf, to_read, &bw, portMAX_DELAY);
        } else {
            // True underrun (avail == 0)
            writeSilence(false);
            if (ws_response_done) {
                CPRINTLN("Speaker: Playback finished (response.done).");
                g_audio_level = 0;
                setAssistantState(STATE_READY_FOR_INPUT);
                g_current_emotion = EMOTION_NEUTRAL;
                ws_response_done = false;
            }
        }
    } else {
        // Play silence to maintain DMA pace
        writeSilence(true);
    }
}

// =====================================================================
// WiFi, mDNS, NTP
// =====================================================================

void start_soft_ap() {
  ap_mode = true;
  setAssistantState(STATE_WIFI_CONFIG);
  uint8_t mac[6];
  char ap_ssid[18];
  WiFi.macAddress(mac);
  snprintf(ap_ssid, sizeof(ap_ssid), "VOICE-AGENT-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(ap_ssid);
  IPAddress ap_ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ap_ip);
}

// Returns true if connected, false if timed out or no credentials
static bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  preferences.begin("wifi-creds", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid.length() == 0) {
    CPRINTLN("NVS empty... Starting AP Config mode.");
    return false;
  }
  CPRINTLN("Found credentials in NVS. Trying to connect to: " + ssid);

  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS) {
    CPRINT(".");
#if USE_EMOJIS
    // Animate pulsing circle while waiting for WiFi
    for (int i = 0; i < 10; i++) {   // 10 × 50ms = 500ms total (same as old delay)
        drawWifiPulse();
        delay(50);
    }
#else
    delay(500);
#endif
  }

  if (WiFi.status() != WL_CONNECTED) {
    CPRINTLN("\nConnection failed or timed out.");
    if (preferences.isKey("ssid")) {
      CPRINTLN("Clearing invalid credentials from NVS.");
      preferences.begin("wifi-creds", false);
      preferences.clear();
      preferences.end();
    }
    return false;
  }

  CPRINTLN("\nWiFi Connected!");
  CPRINT("IP Address: ");
  CPRINTLN(WiFi.localIP());
  return true;
}

static void setupMDNS() {
  uint8_t mac[6];
  char hostname[18];
  WiFi.macAddress(mac);
  snprintf(hostname, sizeof(hostname), "voice-agent-%02x%02x", mac[4], mac[5]);

  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    CPRINT("mDNS started. Web config at: http://");
    CPRINT(hostname);
    CPRINTLN(".local");
  } else {
    CPRINTLN("Error setting up mDNS responder!");
  }
}

static void syncTime() {
  CPRINTLN("\nSyncing time via NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  time_t now = 0;
  uint8_t retries = 0;
  while (now < 100000 && retries < 20) {
    retries++;
#if USE_EMOJIS
    for (int i = 0; i < 10; i++) {
        drawWifiPulse();
        delay(50);
    }
#else
    delay(500);
#endif
    now = time(nullptr);
  }
  if (now > 100000) {
    CPRINTLN("Time synced!");
  } else {
    CPRINTLN("Time sync failed, TLS might have issues.");
  }
}

void init_wifi_and_time() {
  setAssistantState(STATE_WIFI_WAIT);
  CPRINTLN("=== WiFi INIT ===");

  if (!connectToWiFi()) {
    CPRINTLN("Falling back to AP Config Mode.");
    WiFi.mode(WIFI_AP_STA);
    start_soft_ap();
    setup_web_server();
    return;
  }

  setupMDNS();
  syncTime();
#if USE_EMOJIS
  clearWifiPulse();
#endif
  esp_wifi_set_ps(WIFI_PS_NONE);  // High performance mode — disable power save for low latency
  setup_web_server();
}

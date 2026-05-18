// =====================================================================
// WiFi, mDNS, NTP
// =====================================================================

static char s_ap_ssid[18] = "";   // stored so init_wifi_and_time can pass it to the display

void start_soft_ap() {
  ap_mode = true;
  setAssistantState(STATE_WIFI_CONFIG);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(s_ap_ssid, sizeof(s_ap_ssid), "VOICE-AGENT-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(s_ap_ssid);
  IPAddress ap_ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ap_ip);
#if USE_DISPLAY
  display_boot_status("Hosting my own party!");
#endif
}

// Returns true if connected, false if timed out or no credentials
static bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

#if USE_DISPLAY
  display_boot_status("Checking the closet");
#endif

  preferences.begin("wifi-creds", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid.length() == 0) {
    CPRINTLN("NVS empty... Starting AP Config mode.");
    return false;
  }
  CPRINTLN("Found credentials in NVS. Trying to connect to: " + ssid);

#if USE_DISPLAY
  display_boot_status("Connecting with friends");
#endif

  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS) {
    CPRINT(".");
    delay(500);
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
#if USE_DISPLAY
  display_boot_status("Found them!");
#endif
  return true;
}

static void setupMDNS() {
#if USE_DISPLAY
  display_boot_status("Claiming my parking spot");
#endif
  uint8_t mac[6];
  char hostname[18];
  WiFi.macAddress(mac);
  snprintf(hostname, sizeof(hostname), "voice-agent-%02x%02x", mac[4], mac[5]);

  if (MDNS.begin(hostname)) {
    MDNS.setInstanceName("Assistant 5");
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
#if USE_DISPLAY
  display_boot_status("Checking what time it is");
#endif
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  time_t now = 0;
  uint8_t retries = 0;
  while (now < 100000 && retries < 20) {
    retries++;
    delay(500);
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
#if USE_DISPLAY
    display_captive_portal_screen(s_ap_ssid);
#endif
    return;
  }

  setupMDNS();
  syncTime();
  esp_wifi_set_ps(WIFI_PS_NONE);  // High performance mode — disable power save for low latency
  setup_web_server();
}

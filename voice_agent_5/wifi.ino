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

// =====================================================================
// SAVED NETWORK STORE — NVS persistence
// =====================================================================
// The pure store logic lives in wifi_store.h; only the NVS round-trip and the
// one-time migration from the legacy single-credential layout are here.

// Returns false when the blob did not reach flash — NVS full, essentially.
// Callers that are answering a user action must surface that rather than
// reporting a save that will be gone after the next reboot. Note this reports a
// write that did not happen; it never removes anything, so the "a failure must
// never delete credentials" rule is untouched.
bool wifi_store_persist() {
  preferences.begin("wifi-creds", false);
  size_t written = preferences.putBytes("nets", &g_wifi_store, sizeof(g_wifi_store));
  preferences.end();
  if (written != sizeof(g_wifi_store)) {
    CPRINTF("NVS: Wi-Fi store write FAILED (%u of %u bytes) — NVS full\n",
            (unsigned)written, (unsigned)sizeof(g_wifi_store));
    return false;
  }
  return true;
}

// Migrate the pre-multi-network layout: a bare "ssid"/"pass" string pair becomes
// slot 0 of the blob. Mirrors the sysPrompt string->blob migration in
// load_agent_config(). Returns true if anything was migrated.
static bool wifi_store_migrate_legacy() {
  preferences.begin("wifi-creds", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid.length() == 0) return false;

  wifi_store_init(g_wifi_store);
  wifi_store_upsert(g_wifi_store, ssid.c_str(), pass.c_str(), nullptr);
  // Treat the migrated network as the most recently used one so it stays the
  // first pick on the boot after the upgrade.
  wifi_store_mark_used(g_wifi_store, 0, 0);
  wifi_store_persist();

  preferences.begin("wifi-creds", false);
  preferences.remove("ssid");
  preferences.remove("pass");
  preferences.end();

  CPRINTF("NVS: migrated legacy Wi-Fi credentials for '%s' into slot 0\n", ssid.c_str());
  return true;
}

void wifi_store_load() {
  wifi_store_init(g_wifi_store);

  preferences.begin("wifi-creds", true);
  size_t len = preferences.getBytesLength("nets");
  bool have_blob = (len == sizeof(WifiStore));
  if (have_blob) {
    preferences.getBytes("nets", &g_wifi_store, sizeof(WifiStore));
  }
  preferences.end();

  if (!have_blob) {
    if (len > 0) {
      CPRINTF("NVS: Wi-Fi blob is %u bytes, expected %u — discarding\n",
              (unsigned)len, (unsigned)sizeof(WifiStore));
    }
    if (!wifi_store_migrate_legacy()) wifi_store_init(g_wifi_store);
    return;
  }

  if (!wifi_store_is_valid(g_wifi_store)) {
    // Run empty, but deliberately DON'T overwrite the stored blob. A validation
    // failure is most likely a firmware-side mistake (a version bumped without a
    // migration, a changed layout), and persisting here would turn a recoverable
    // bad read into permanent loss of every saved network. Leaving the bytes
    // alone means fixing the firmware brings them back. The blob is only
    // rewritten when the user actually saves a network.
    CPRINTLN("NVS: saved Wi-Fi store failed validation — running with none. "
             "Stored blob left untouched so a firmware fix can recover it.");
    wifi_store_init(g_wifi_store);
    return;
  }
  wifi_store_normalize(g_wifi_store);
}

// ---- One-shot "connect to this on the next boot" target ----------------
// Written by POST /api/wifi/connect, consumed once by the boot cascade.
//
// Kept OUT of the WifiStore blob on purpose: it is a transient command, not part
// of the saved set, and giving it a field would change the blob layout and force
// a version bump plus a migration for something that lives for exactly one boot.
static const char* WIFI_PENDING_KEY = "pending";

// Returns false if the target did not reach flash. That matters more than it
// looks: the reboot below would then come up and rejoin the network the user
// just asked to leave — the same silent veto the scan filter is forbidden from
// applying.
bool wifi_set_pending_target(const char* ssid) {
  preferences.begin("wifi-creds", false);
  bool ok = (preferences.putString(WIFI_PENDING_KEY, ssid) > 0);
  preferences.end();
  if (!ok) CPRINTF("NVS: pending Wi-Fi target '%s' write FAILED — NVS full\n", ssid);
  return ok;
}

// Read and clear in one go. Clearing BEFORE the attempt is deliberate: a target
// that never comes up must not be blind-retried on every future boot.
static String wifi_take_pending_target() {
  preferences.begin("wifi-creds", false);
  String s = preferences.getString(WIFI_PENDING_KEY, "");
  if (s.length()) preferences.remove(WIFI_PENDING_KEY);
  preferences.end();
  return s;
}

// =====================================================================
// BOOT CONNECT CASCADE
// =====================================================================
// Scan, then try the saved networks that are actually in range, most recently
// used first. A failed attempt NEVER deletes the entry: failures are usually
// transient (AP rebooting, DHCP hiccup), and the old wipe-on-failure behavior is
// exactly what made moving between networks painful.
//
// One association attempt against a saved slot. On success, records it as the
// active slot and stamps it most-recently-used. On failure, leaves the stored
// credentials alone and drops the radio ready for the next attempt.
static bool wifi_try_slot(int idx, unsigned long budget) {
  WifiNet& net = g_wifi_store.nets[idx];
  CPRINTF("Trying '%s' (budget %lu ms)...\n", net.ssid, budget);
#if USE_DISPLAY
  {
    char line[WIFI_BOOT_LINE_MAX];
    if (strlen(net.ssid) > WIFI_BOOT_SSID_MAX) {
      snprintf(line, sizeof(line), "Hailing %.*s...", WIFI_BOOT_SSID_MAX - 3, net.ssid);
    } else {
      snprintf(line, sizeof(line), "Hailing %s", net.ssid);
    }
    display_boot_status(line);
  }
#endif

  WiFi.begin(net.ssid, net.pass);
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < budget) {
    CPRINT(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    g_wifi_active_slot = idx;
    wifi_store_mark_used(g_wifi_store, idx, 0);  // epoch stamped after syncTime()
    wifi_store_persist();
    CPRINTLN("\nWiFi Connected!");
    CPRINT("IP Address: ");
    CPRINTLN(WiFi.localIP());
#if USE_DISPLAY
    display_boot_status("Found them!");
#endif
    return true;
  }

  CPRINTF("\n'%s' did not answer (WiFi.status()=%d after %lu ms) — keeping the credentials.\n",
          net.ssid, (int)WiFi.status(), (unsigned long)(millis() - startAttempt));
  WiFi.disconnect(true, true);
  delay(100);
  return false;
}

// Returns true if connected, false if there is nothing to try or everything failed.
static bool connectToWiFi() {
  // Escape hatch: holding PTT at power-on skips straight to the captive portal,
  // so an unreachable saved network never costs you the full cascade.
  // pinMode(BUTTON_PIN, INPUT_PULLUP) already ran in setup(); the button is active low.
  if (digitalRead(BUTTON_PIN) == LOW) {
    CPRINTLN("PTT held at boot — skipping Wi-Fi and opening the config portal.");
#if USE_DISPLAY
    display_boot_status("Skipping the search");
#endif
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

#if USE_DISPLAY
  display_boot_status("Checking the closet");
#endif

  if (g_wifi_store.count == 0) {
    CPRINTLN("No saved networks... Starting AP Config mode.");
    return false;
  }
  CPRINTF("Saved networks: %u (next_seq=%lu)\n",
          (unsigned)g_wifi_store.count, (unsigned long)g_wifi_store.next_seq);
  for (int i = 0; i < g_wifi_store.count; i++) {
    CPRINTF("  slot %d: '%s' use_seq=%lu last_epoch=%lu\n", i,
            g_wifi_store.nets[i].ssid,
            (unsigned long)g_wifi_store.nets[i].use_seq,
            (unsigned long)g_wifi_store.nets[i].last_epoch);
  }

  // An explicit "Connect to X" from the portal is honoured DIRECTLY: X is attempted
  // first and without requiring it to show up in a scan. The scan filter below is a
  // convenience for choosing among saved networks on an ordinary boot — it must not
  // veto a request the user just made by hand. A hotspot that has dozed off, a radio
  // that missed a beacon, an AP on a crowded channel: none of those should silently
  // land the device back on the network it was told to leave. If X doesn't come up,
  // we simply fall through to the usual cascade.
  int pending_idx = -1;
  String pending = wifi_take_pending_target();
  if (pending.length() > 0) {
    // Remember what was asked for either way, so the portal can say what happened
    // once it is reachable again. A silent fallback is the thing that makes this
    // feature feel broken.
    g_wifi_request_ssid = pending;
    g_wifi_request_ok   = false;
    pending_idx = wifi_store_find(g_wifi_store, pending.c_str());
    if (pending_idx >= 0) {
      CPRINTF("Requested network '%s' — attempting it first, no scan required.\n",
              pending.c_str());
      if (wifi_try_slot(pending_idx, WIFI_SOLE_TIMEOUT_MS)) {
        g_wifi_request_ok = true;
        return true;
      }
      CPRINTF("Requested network '%s' did not come up; falling back to the usual order.\n",
              pending.c_str());
    } else {
      CPRINTF("Requested network '%s' is no longer saved — ignoring.\n", pending.c_str());
    }
  }

  // Blocking scan is safe HERE AND ONLY HERE: this runs inside setup(), before
  // loop() and before the protocol task exists, so nothing is starved. The portal's
  // scan endpoint must use the async form instead.
#if USE_DISPLAY
  display_boot_status("Scanning for friends");
#endif
  int found = WiFi.scanNetworks();
  CPRINTF("Scan found %d networks\n", found);
  for (int j = 0; j < found; j++) {
    CPRINTF("  scan %d: '%s' rssi=%d ch=%d\n", j,
            WiFi.SSID(j).c_str(), (int)WiFi.RSSI(j), (int)WiFi.channel(j));
  }

  // Candidates = saved entries whose SSID is in the scan, most recently used first.
  uint8_t order[WIFI_MAX_NETWORKS];
  int ordered = wifi_store_order_by_recency(g_wifi_store, order);
  uint8_t candidates[WIFI_MAX_NETWORKS];
  int num_candidates = 0;
  for (int i = 0; i < ordered; i++) {
    if ((int)order[i] == pending_idx) continue;   // already attempted above, and it failed
    const char* ssid = g_wifi_store.nets[order[i]].ssid;
    bool in_scan = false;
    for (int j = 0; j < found; j++) {
      if (WiFi.SSID(j) == ssid) { in_scan = true; break; }
    }
    if (in_scan) {
      candidates[num_candidates++] = order[i];
    } else {
      // The single most confusing failure this feature can produce: a network you
      // explicitly asked for is dropped here and the device silently joins another.
      // Say so out loud.
      CPRINTF("  SKIP '%s' (use_seq=%lu) — not in scan results\n",
              ssid, (unsigned long)g_wifi_store.nets[order[i]].use_seq);
    }
  }
  WiFi.scanDelete();

  CPRINTF("Candidates in try order: ");
  for (int c = 0; c < num_candidates; c++) {
    CPRINTF("%s'%s'", c ? " -> " : "", g_wifi_store.nets[candidates[c]].ssid);
  }
  CPRINTF("%s\n", num_candidates ? "" : "(none)");

  if (num_candidates == 0) {
    CPRINTLN("None of the saved networks are in range.");
#if USE_DISPLAY
    display_boot_status("Nobody I know here");
#endif
    return false;
  }

  unsigned long budget = (num_candidates == 1) ? WIFI_SOLE_TIMEOUT_MS : WIFI_CONNECT_TIMEOUT_MS;

  for (int c = 0; c < num_candidates; c++) {
    CPRINTF("Candidate %d of %d: ", c + 1, num_candidates);
    if (wifi_try_slot(candidates[c], budget)) return true;
  }

  CPRINTLN("Every saved network in range failed.");
  return false;
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

  // Load before connecting, not inside connectToWiFi(): every exit path — including
  // the PTT-held shortcut — leads to a portal that needs the saved list populated.
  wifi_store_load();

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

  // Stamp the wall clock on the slot we connected through, now that NTP has run.
  // Ordering never depends on this (use_seq does) — it is only so the portal can
  // show "last used <date>".
  if (g_wifi_active_slot >= 0) {
    time_t now = time(nullptr);
    if (now > 100000) {
      g_wifi_store.nets[g_wifi_active_slot].last_epoch = (uint32_t)now;
      wifi_store_persist();
    }
  }

  esp_wifi_set_ps(WIFI_PS_NONE);  // High performance mode — disable power save for low latency
  setup_web_server();
}

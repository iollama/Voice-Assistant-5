// =====================================================================
// NVS Config & WebServer Handlers
// =====================================================================

void load_agent_config() {
  preferences.begin("agentConfig", true);
  g_sys_instruction = preferences.getString("sysPrompt", DEFAULT_SYS_INSTRUCTION);
  g_temperature = preferences.getFloat("temp", DEFAULT_TEMPERATURE);
  g_persist_conversation = preferences.getBool("persist", DEFAULT_PERSIST_CONVO);
  g_verbose_logging = preferences.getBool("verbose", DEFAULT_VERBOSE_LOGGING);
  g_api_key = preferences.getString("apiKey", "");
  g_volume = preferences.getInt("volume", 50);
  if (g_volume < 0) g_volume = 0;
  if (g_volume > 100) g_volume = 100;
  preferences.end();
}

void handle_root() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
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
        textarea { resize: vertical; min-height: 100px; }
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
  html += "<div class='form-group'><label for='sysPrompt'>System Prompt</label><textarea id='sysPrompt' name='sysPrompt' maxlength='2000' required>" + g_sys_instruction + "</textarea></div>";
  html += "<div class='form-group'><label for='temp'>Temperature</label><input type='number' id='temp' name='temp' min='0.0' max='2.0' step='0.1' value='" + String(g_temperature, 2) + "' required></div>";
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

  html += "</div></body></html>";
  server.send(200, "text/html", html);
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
  preferences.begin("agentConfig", false);
  g_sys_instruction = server.arg("sysPrompt");
  g_temperature = server.arg("temp").toFloat();
  g_persist_conversation = server.hasArg("persist");
  g_verbose_logging = server.hasArg("verbose");
  preferences.putString("sysPrompt", g_sys_instruction);
  preferences.putFloat("temp", g_temperature);
  preferences.putBool("persist", g_persist_conversation);
  preferences.putBool("verbose", g_verbose_logging);
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

void setup_web_server() {
  server.on("/", HTTP_GET, handle_root);
  server.on("/save", HTTP_POST, handle_save);
  server.on("/delete", HTTP_POST, handle_delete);
  server.on("/saveAgent", HTTP_POST, handle_save_agent);
  server.on("/restoreAgent", HTTP_POST, handle_restore_agent);
  server.on("/saveApiKey", HTTP_POST, handle_save_api_key);
  server.on("/clearApiKey", HTTP_POST, handle_clear_api_key);
  server.on("/saveVolume", HTTP_POST, handle_save_volume);

  server.onNotFound([]() {
    if (ap_mode) {
      server.send(302, "text/plain", "http://" + WiFi.softAPIP().toString());
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
}

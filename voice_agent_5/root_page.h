// HTML+CSS+JS for the captive-portal main settings page (/).
// Kept in a .h file for the same reason as emojis_page.h: the Arduino
// auto-prototyper scans .ino files for C-style declarations and chokes on the
// embedded JavaScript (`async function`, arrow functions) inside the raw
// string. .h files are not scanned, so the JS is safe verbatim here.
//
// The page is fully static and served via server.send_P (PROGMEM, no heap).
// All live values are fetched at load time from GET /api/config and injected by
// the script below. Settings are saved with normal form POSTs to the existing
// handlers (handle_save, handle_save_agent, handle_save_volume, ...), so this
// file owns presentation only.

#pragma once

static const char ROOT_PAGE_HTML[] PROGMEM = R"=====(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Voice Agent - Settings</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
*{box-sizing:border-box}
body{font-family:"Comic Sans MS","Arial Black",system-ui,sans-serif;color:#111;margin:0;padding:18px;
     background:#fff200;background-image:radial-gradient(#0000001a 18%,transparent 19%);background-size:15px 15px}
.wrap{max-width:600px;margin:0 auto}
a{color:#1742d1}
.hd{position:relative;background:#ff2d2d;color:#fff;border:4px solid #111;border-radius:16px 16px 16px 3px;
    padding:16px 18px;font-weight:900;font-size:26px;text-shadow:2px 2px 0 #111;box-shadow:7px 7px 0 #111}
.hd small{display:block;font-size:13px;font-weight:700;color:#ffe34d;margin-top:3px;text-shadow:1px 1px 0 #111}
.statusline{margin:14px 2px;font-weight:700;font-size:14px}
.zone{display:inline-block;background:#1742d1;color:#fff;border:4px solid #111;border-radius:30px;
      padding:6px 16px;font-weight:900;font-size:14px;text-transform:uppercase;box-shadow:4px 4px 0 #111;
      margin:22px 0 6px;transform:rotate(-1.5deg)}
.zone.admin-pill{background:#111;transform:rotate(1.5deg)}
.card{background:#fff;border:4px solid #111;border-radius:12px;padding:14px;margin-top:14px;box-shadow:6px 6px 0 #111}
.card h4{margin:0 0 10px;font-size:16px;font-weight:900;color:#1742d1}
label{display:block;font-size:12px;font-weight:900;text-transform:uppercase;color:#111;margin:0 0 5px}
input[type=text],input[type=password],textarea,select{width:100%;border:3px solid #111;border-radius:8px;
      padding:9px;font:inherit;font-size:14px;background:#fff}
textarea{min-height:120px;resize:vertical}
.hint{font-size:11px;font-weight:700;color:#666;margin-top:4px}
.btn{display:inline-block;margin-top:14px;background:#1742d1;color:#fff;border:4px solid #111;border-radius:30px;
     padding:10px 20px;font-weight:900;text-transform:uppercase;font-size:13px;box-shadow:5px 5px 0 #111;cursor:pointer}
.btn:active{transform:translate(2px,2px);box-shadow:3px 3px 0 #111}
.btn[disabled]{opacity:.45;cursor:not-allowed;box-shadow:3px 3px 0 #777}
.btn.go{background:#42c98e;color:#111}
.btn.danger{background:#ff2d2d}
.btn.ghost{background:#fff;color:#111}
.btn.sm{font-size:12px;padding:8px 14px;margin-top:10px}
.vol{display:flex;align-items:center;gap:12px}
.vol input[type=range]{flex:1;accent-color:#ff2d2d;height:8px}
.vol b{background:#42c98e;color:#111;border:3px solid #111;border-radius:8px;padding:4px 9px;font-size:14px;min-width:54px;text-align:center}
.seg{display:flex;border:3px solid #111;border-radius:8px;overflow:hidden}
.seg input{display:none}
.seg label{flex:1;margin:0;text-align:center;padding:10px;font-weight:900;font-size:13px;cursor:pointer;text-transform:none}
.seg input:checked+label{background:#ffe34d}
.seg label+input+label{border-left:3px solid #111}
.row{display:flex;align-items:center;gap:8px;margin-top:6px}
.row label{margin:0;text-transform:none;font-size:14px;font-weight:700}
.row input[type=checkbox]{width:18px;height:18px}
.emoji-row{display:flex;align-items:center;justify-content:space-between;gap:10px}
.tok{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:4px}
.tok div{background:#fffbe6;border:3px solid #111;border-radius:8px;padding:8px;font-size:12px;font-weight:700}
.tok b{display:block;font-size:18px;color:#1742d1}
.tok .sub{font-size:10px;color:#666;font-weight:700}
/* ---- admin gate: CSS checkbox-hack (works without JS; JS only sets it open in AP mode) ---- */
.gate-cb{display:none}
.lockbar{display:flex;align-items:center;gap:14px;background:#111;color:#ffe34d;border:4px solid #111;border-radius:12px;
         padding:14px 16px;cursor:pointer;box-shadow:6px 6px 0 #ff2d2d;margin:0}
.lockbar .pad{font-size:30px;line-height:1}
.lockbar .t{font-weight:900;font-size:16px;text-transform:uppercase;flex:1}
.lockbar .t small{display:block;font-size:11px;color:#bbb;font-weight:700;text-transform:none}
.gate-cb:not(:checked)~.lockbar .pad:before{content:"\01F512"}
.gate-cb:checked~.lockbar .pad:before{content:"\01F513"}
.gate-cb:checked~.lockbar{box-shadow:6px 6px 0 #42c98e}
.admin{max-height:0;overflow:hidden}
.gate-cb:checked~.admin{max-height:none;overflow:visible;margin-top:4px}
.footer{text-align:center;margin:28px 0 6px;font-size:13px;font-weight:700;color:#111}
.footer a{font-weight:900}
</style></head><body>
<div class="wrap">

  <div class="hd">Voice Agent<small>Set me up below!</small></div>
  <div class="statusline" id="statusline">Loading&hellip;</div>

  <!-- ============ USER ZONE ============ -->
  <div class="zone">&#127899;&#65039; Your Settings</div>

  <form class="card" action="/saveAgent" method="POST">
    <h4>&#129504; Personality</h4>
    <label for="sysPrompt">Persona</label>
    <textarea id="sysPrompt" name="sysPrompt" dir="auto" maxlength="12288" required></textarea>
    <div class="hint">Up to 12,288 characters. Press Ctrl+Shift to switch typing direction (LTR/RTL).</div>
    <label for="voice" style="margin-top:12px">Voice</label>
    <select id="voice" name="voice"></select>
    <button type="submit" class="btn go save" disabled>Save personality &#9656;</button>
  </form>

  <form class="card" action="/saveVolume" method="POST">
    <h4>&#128266; Volume</h4>
    <div class="vol">
      <input type="range" id="volume" name="volume" min="0" max="100" value="50"
             oninput="document.getElementById('volVal').textContent=this.value+'%'">
      <b id="volVal">50%</b>
    </div>
    <button type="submit" class="btn go save" disabled>Save volume &#9656;</button>
  </form>

  <form class="card" action="/saveAudioOut" method="POST">
    <h4>&#127911; Audio Output</h4>
    <div class="seg">
      <input type="radio" name="audioOut" id="ao0" value="0" checked><label for="ao0">&#128264; Speaker</label>
      <input type="radio" name="audioOut" id="ao1" value="1"><label for="ao1">&#127911; Headphones</label>
    </div>
    <div class="hint">Headphones routes audio to the 3.5 mm jack for an external speaker. Only use it if one is connected.</div>
    <button type="submit" class="btn save" disabled>Save audio output &#9656;</button>
  </form>

  <div class="card emoji-row" id="emojiCard" style="display:none">
    <div><h4 style="margin:0">&#128512; Emoji Faces</h4><div class="hint">Customize the on-device animations</div></div>
    <a class="btn" style="margin:0" href="/emojis">Open editor &#9656;</a>
  </div>

  <!-- ============ ADMIN ZONE ============ -->
  <div class="zone admin-pill">&#128295; Admin Zone</div>
  <input type="checkbox" id="adminToggle" class="gate-cb">
  <label class="lockbar" for="adminToggle">
    <span class="pad"></span>
    <span class="t">Unlock admin settings<small>Network &middot; API key &middot; diagnostics &middot; (not a real password)</small></span>
  </label>

  <div class="admin">
    <form class="card" action="/save" method="POST">
      <h4>&#128246; Wi-Fi Network</h4>
      <label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" required>
      <label for="pass" style="margin-top:10px">Password</label><input type="password" id="pass" name="pass">
      <button type="submit" class="btn">Save &amp; reboot &#9656;</button>
    </form>
    <form action="/delete" method="POST" onsubmit="return confirm('Forget this network and restart?');" style="margin-top:-4px">
      <button type="submit" class="btn danger sm">Forget network</button>
    </form>

    <form class="card" action="/saveApiKey" method="POST">
      <h4>&#128273; OpenAI API Key</h4>
      <div class="hint" id="keyStatus" style="margin-bottom:8px">&hellip;</div>
      <label for="apiKey">New key</label><input type="password" id="apiKey" name="apiKey" placeholder="sk-..." required>
      <button type="submit" class="btn">Save key &#9656;</button>
    </form>
    <form action="/clearApiKey" method="POST" onsubmit="return confirm('Clear saved key and revert to the compiled default?');" style="margin-top:-4px">
      <button type="submit" class="btn ghost sm">Clear saved key</button>
    </form>

    <form class="card" action="/saveBehavior" method="POST">
      <h4>&#9881;&#65039; Behavior</h4>
      <div class="row"><input type="checkbox" id="persist" name="persist" value="true"><label for="persist">Persist conversation across turns</label></div>
      <div class="row"><input type="checkbox" id="verbose" name="verbose" value="true"><label for="verbose">Verbose serial logging</label></div>
      <button type="submit" class="btn go save" disabled>Save behavior &#9656;</button>
    </form>
    <form action="/restoreAgent" method="POST" onsubmit="return confirm('Restore default agent settings (prompt, voice, behavior)?');" style="margin-top:-4px">
      <button type="submit" class="btn ghost sm">Restore defaults</button>
    </form>

    <div class="card">
      <h4>&#128202; Token Usage <span class="hint" style="font-weight:700">(since boot &middot; reload to refresh)</span></h4>
      <div class="tok">
        <div>Total<b id="tkTotal">0</b></div>
        <div>Input<b id="tkInput">0</b><span class="sub" id="tkInputSub"></span></div>
        <div>Output<b id="tkOutput">0</b><span class="sub" id="tkOutputSub"></span></div>
        <div>Cached input<b id="tkCached">0</b></div>
      </div>
    </div>
  </div>

  <div class="footer">developed by <a href="https://www.iollama.com/" target="_blank" rel="noopener">iollama</a></div>
</div>

<script>
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
const fmt = n => Number(n||0).toLocaleString();

async function loadConfig(){
  let c;
  try{
    c = await (await fetch('/api/config',{cache:'no-store'})).json();
  }catch(e){
    $('statusline').textContent = 'Could not load settings: ' + e.message;
    return;
  }

  // Connection status + AP-mode onboarding
  if (c.apMode){
    $('statusline').innerHTML = "Let's get you online &#128071; pop your Wi-Fi in below.";
    $('adminToggle').checked = true;           // open admin so Wi-Fi is reachable on first boot
  } else {
    $('statusline').innerHTML = 'Connected to <b>' + esc(c.ssid || '?') + '</b> &#10003;';
  }

  // Personality
  $('sysPrompt').value = c.sysPrompt || '';
  const sel = $('voice');
  sel.innerHTML = '';
  (c.voices || []).forEach(v=>{
    const o = document.createElement('option');
    o.value = v.id; o.textContent = v.label;
    if (v.id === c.voice) o.selected = true;
    sel.appendChild(o);
  });

  // Volume
  $('volume').value = c.volume;
  $('volVal').textContent = c.volume + '%';

  // Audio output
  $(c.audioOut === 1 ? 'ao1' : 'ao0').checked = true;

  // Emoji card (only when the display build is present)
  if (c.useDisplay) $('emojiCard').style.display = 'flex';

  // Admin: key status + behavior
  $('keyStatus').innerHTML = c.apiKeySet
    ? '<span style="color:#42c98e">&#9679; Custom key is set</span>'
    : '<span style="color:#666">&#9679; Using compiled default key</span>';
  $('persist').checked = !!c.persist;
  $('verbose').checked = !!c.verbose;

  // Token usage
  const t = c.tokens || {};
  $('tkTotal').textContent  = fmt(t.total);
  $('tkInput').textContent  = fmt(t.input);
  $('tkOutput').textContent = fmt(t.output);
  $('tkCached').textContent = fmt(t.inputCached);
  $('tkInputSub').textContent  = 'text ' + fmt(t.inputText) + ' / audio ' + fmt(t.inputAudio);
  $('tkOutputSub').textContent = 'text ' + fmt(t.outputText) + ' / audio ' + fmt(t.outputAudio);

  // Settings are ready: enable saves
  document.querySelectorAll('.save').forEach(b=> b.disabled = false);
}
loadConfig();
</script>
</body></html>)=====";

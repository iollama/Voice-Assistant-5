// HTML+CSS+JS for the captive-portal main settings page (/).
// Kept in a .h file for the same reason as emojis_page.h: the Arduino
// auto-prototyper scans .ino files for C-style declarations and chokes on the
// embedded JavaScript (`async function`, arrow functions) inside the raw
// string. .h files are not scanned, so the JS is safe verbatim here.
//
// The page is fully static and served via server.send_P (PROGMEM, no heap).
// All live values are fetched at load time from GET /api/config and injected by
// the script below. Most settings are saved with normal form POSTs to the
// existing handlers (handle_save_agent, handle_save_volume, ...); the saved
// Wi-Fi network list is the one card driven entirely by fetch() against the
// /api/wifi* endpoints, since it renders a list rather than a single value.
// Either way this file owns presentation only.

#pragma once

#include "theme_css.h"
#include "portal_js.h"

static const char ROOT_PAGE_HTML[] PROGMEM = R"=====(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Voice Agent - Settings</title>
<meta name="viewport" content="width=device-width,initial-scale=1">)=====" THEME_CSS R"=====(<style>
.wrap{max-width:600px;margin:0 auto}
.hd{position:relative;background:var(--red);color:#fff;border:4px solid var(--ink);border-radius:16px 16px 16px 3px;
    padding:16px 18px;font-weight:900;font-size:26px;text-shadow:2px 2px 0 var(--ink);box-shadow:7px 7px 0 var(--ink)}
.hd small{display:block;font-size:13px;font-weight:700;color:var(--yellow);margin-top:3px;text-shadow:1px 1px 0 var(--ink)}
.statusline{margin:14px 2px;font-weight:700;font-size:14px}
.zone{display:inline-block;background:var(--blue);color:#fff;border:4px solid var(--ink);border-radius:30px;
      padding:6px 16px;font-weight:900;font-size:14px;text-transform:uppercase;box-shadow:4px 4px 0 var(--ink);
      margin:22px 0 6px;transform:rotate(-1.5deg)}
.zone.admin-pill{background:var(--ink);transform:rotate(1.5deg)}
.card{background:#fff;border:4px solid var(--ink);border-radius:12px;padding:14px;margin-top:14px;box-shadow:6px 6px 0 var(--ink)}
.card h4{margin:0 0 10px;font-size:16px;font-weight:900;color:var(--blue)}
label{display:block;font-size:12px;font-weight:900;text-transform:uppercase;color:var(--ink);margin:0 0 5px}
input[type=text],input[type=password],textarea,select{width:100%;border:3px solid var(--ink);border-radius:8px;
      padding:9px;font:inherit;font-size:14px;background:#fff}
textarea{min-height:120px;resize:vertical}
.hint{font-size:11px;font-weight:700;color:var(--mute);margin-top:4px}
.btn{display:inline-block;margin-top:14px;background:var(--blue);color:#fff;border:4px solid var(--ink);border-radius:30px;
     padding:10px 20px;font-weight:900;text-transform:uppercase;font-size:13px;box-shadow:5px 5px 0 var(--ink);cursor:pointer}
.btn:active{transform:translate(2px,2px);box-shadow:3px 3px 0 var(--ink)}
.btn[disabled]{opacity:.45;cursor:not-allowed;box-shadow:3px 3px 0 #777}
.btn.go{background:var(--green);color:var(--ink)}
.btn.danger{background:var(--red)}
.btn.ghost{background:#fff;color:var(--ink)}
.btn.sm{font-size:12px;padding:8px 14px;margin-top:10px}
.vol{display:flex;align-items:center;gap:12px}
.vol input[type=range]{flex:1;accent-color:var(--red);height:8px}
.vol b{background:var(--green);color:var(--ink);border:3px solid var(--ink);border-radius:8px;padding:4px 9px;font-size:14px;min-width:54px;text-align:center}
.seg{display:flex;border:3px solid var(--ink);border-radius:8px;overflow:hidden}
.seg input{display:none}
.seg label{flex:1;margin:0;text-align:center;padding:10px;font-weight:900;font-size:13px;cursor:pointer;text-transform:none}
.seg input:checked+label{background:var(--yellow)}
.seg label+input+label{border-left:3px solid var(--ink)}
.row{display:flex;align-items:center;gap:8px;margin-top:6px}
.row label{margin:0;text-transform:none;font-size:14px;font-weight:700}
.row input[type=checkbox]{width:18px;height:18px}
.emoji-row{display:flex;align-items:center;justify-content:space-between;gap:10px}
.ioCat{margin-top:10px}
.ioCatHdr{width:100%;text-align:left;background:var(--blue);color:#fff;border:3px solid var(--ink);border-radius:8px;padding:9px 12px;font-weight:900;font-size:13px;text-transform:uppercase;cursor:pointer;box-shadow:3px 3px 0 var(--ink)}
.ioCatHdr:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--ink)}
.ioCaret{display:inline-block;transition:transform .12s;margin-right:4px}
.ioCatHdr.open .ioCaret{transform:rotate(90deg)}
.ioCatN{float:right;opacity:.85;font-weight:700}
.ioGrid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:8px}
.ioTile{border:3px solid var(--ink);border-radius:10px;background:#fff;padding:8px;text-align:center;cursor:pointer;box-shadow:4px 4px 0 var(--ink)}
.ioTile:active{transform:translate(2px,2px);box-shadow:2px 2px 0 var(--ink)}
.ioTile img{width:100%;aspect-ratio:1;object-fit:cover;border-radius:50%;border:3px solid var(--ink);background:#000;display:block;margin-bottom:6px}
.ioTile .nm{font-weight:900;font-size:12px;color:var(--ink)}
.tok{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:4px}
.tok div{background:#fffbe6;border:3px solid var(--ink);border-radius:8px;padding:8px;font-size:12px;font-weight:700}
.tok b{display:block;font-size:18px;color:var(--blue)}
.tok .sub{font-size:10px;color:var(--mute);font-weight:700}
/* ---- saved Wi-Fi networks list + scan picker ---- */
.wfRow{display:flex;align-items:center;gap:8px;background:#fff;border:3px solid var(--ink);border-radius:8px;
       padding:9px 10px;margin-top:8px;box-shadow:3px 3px 0 var(--ink)}
.wfRow.on{background:#eaffea}
.wfMeta{flex:1;min-width:0}
.wfName{font-weight:900;font-size:14px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.wfSub{font-size:11px;font-weight:700;color:var(--mute);margin-top:2px}
.wfTag{display:inline-block;background:var(--green);color:var(--ink);border:2px solid var(--ink);border-radius:20px;
       padding:1px 7px;font-size:10px;font-weight:900;text-transform:uppercase;margin-left:6px;vertical-align:1px}
.wfTag.new{background:var(--yellow)}
.wfBtn{flex:none;background:var(--blue);color:#fff;border:3px solid var(--ink);border-radius:20px;padding:6px 11px;
       font:inherit;font-weight:900;font-size:11px;text-transform:uppercase;cursor:pointer;box-shadow:3px 3px 0 var(--ink)}
.wfBtn:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--ink)}
.wfBtn[disabled]{opacity:.45;cursor:not-allowed}
.wfBtn.x{background:var(--red)}
.wfCount{float:right;font-size:11px;font-weight:900;color:var(--mute);text-transform:uppercase;margin-top:3px}
.wfPick{border:3px solid var(--ink);border-radius:8px;margin-top:8px;max-height:190px;overflow-y:auto}
.wfPickRow{display:flex;align-items:center;gap:9px;padding:8px 10px;border-bottom:2px solid #eee;
           cursor:pointer;font-size:13px;font-weight:700}
.wfPickRow:last-child{border-bottom:0}
.wfPickRow:active{background:var(--yellow)}
.wfPickRow .nm{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.wfBars{flex:none;font-family:ui-monospace,Menlo,Consolas,monospace;font-weight:900;color:var(--blue)}
.wfAdd{margin-top:14px;border-top:3px dashed var(--ink);padding-top:12px}
/* ---- admin gate: CSS checkbox-hack (works without JS; JS only sets it open in AP mode) ---- */
.gate-cb{display:none}
.lockbar{display:flex;align-items:center;gap:14px;background:var(--ink);color:var(--yellow);border:4px solid var(--ink);border-radius:12px;
         padding:14px 16px;cursor:pointer;box-shadow:6px 6px 0 var(--red);margin:0}
.lockbar .pad{font-size:30px;line-height:1}
.lockbar .t{font-weight:900;font-size:16px;text-transform:uppercase;flex:1}
.lockbar .t small{display:block;font-size:11px;color:#bbb;font-weight:700;text-transform:none}
.gate-cb:not(:checked)~.lockbar .pad:before{content:"\01F512"}
.gate-cb:checked~.lockbar .pad:before{content:"\01F513"}
.gate-cb:checked~.lockbar{box-shadow:6px 6px 0 var(--green)}
.admin{max-height:0;overflow:hidden}
.gate-cb:checked~.admin{max-height:none;overflow:visible;margin-top:4px}
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
    <label for="language" style="margin-top:12px">Language</label>
    <select id="language" name="language"></select>
    <div class="hint">Automatic matches whatever language you speak. Pick a language to make every reply use it.</div>
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

  <div class="card">
    <h4>&#128190; Import / Export Profile</h4>
    <div class="hint" style="margin-bottom:10px">Back up or transfer your persona, voice, volume and emoji as one <b>.zip</b>. Needs the browser to be online. Secrets (Wi-Fi, API key) are never exported.</div>
    <button type="button" id="ioBrowse" class="btn go sm" style="margin-top:0">&#128270; Browse</button>
    <button type="button" id="ioExport" class="btn sm">Export profile &#9656;</button>
    <button type="button" id="ioImportFile" class="btn sm">Import from file</button>
    <div id="ioRepoRow" style="display:none;margin-top:12px">
      <div class="hint" style="margin-bottom:8px">Pick a ready-made persona from the gallery. It replaces your persona text and emoji.</div>
      <div id="ioRepoCatalog"></div>
    </div>
    <input type="file" id="ioFile" accept=".zip" style="display:none">
    <div id="ioStatus" style="display:none;margin-top:12px;padding:9px;border:3px solid var(--ink);border-radius:8px;font-size:13px;font-weight:700;box-shadow:3px 3px 0 var(--ink)"></div>
  </div>

  <!-- ============ ADMIN ZONE ============ -->
  <div class="zone admin-pill">&#128295; Admin Zone</div>
  <input type="checkbox" id="adminToggle" class="gate-cb">
  <label class="lockbar" for="adminToggle">
    <span class="pad"></span>
    <span class="t">Unlock admin settings<small>Network &middot; API key &middot; diagnostics &middot; (not a real password)</small></span>
  </label>

  <div class="admin">
    <div class="card">
      <h4>&#128246; Wi-Fi Networks <span class="wfCount" id="wfCount"></span></h4>
      <div class="hint" style="margin-bottom:4px">Six networks are remembered. At power-on the device joins the
        most recently used one that's in range, and tries the others if that fails.</div>
      <div id="wfList"></div>
      <div id="wfStatus" style="display:none;margin-top:12px;padding:9px;border:3px solid var(--ink);border-radius:8px;font-size:13px;font-weight:700;box-shadow:3px 3px 0 var(--ink)"></div>

      <div class="wfAdd">
        <label for="wfSsid">Add a network</label>
        <input type="text" id="wfSsid" placeholder="Network name (SSID)" autocomplete="off" maxlength="32">
        <button type="button" id="wfScan" class="wfBtn" style="margin-top:10px">&#128269; Scan</button>
        <div id="wfPick" class="wfPick" style="display:none"></div>
        <label for="wfPass" style="margin-top:12px">Password</label>
        <input type="password" id="wfPass" placeholder="Leave empty for an open network" maxlength="63">
        <div class="hint">You can add a network you're nowhere near &mdash; it'll be used the next time it's in range.</div>
        <button type="button" id="wfAdd" class="btn go sm">Add network &#9656;</button>
      </div>
    </div>

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
      <div class="row"><input type="checkbox" id="showHidden" name="showHidden" value="true"><label for="showHidden">Show hidden personas in Browse</label></div>
      <button type="submit" class="btn go save" disabled>Save behavior &#9656;</button>
    </form>
    <form action="/restoreAgent" method="POST" onsubmit="return confirm('Restore default agent settings (prompt, voice, behavior)?');" style="margin-top:-4px">
      <button type="submit" class="btn ghost sm">Restore defaults</button>
    </form>

    <div class="card" id="provCard" style="display:none">
      <h4>&#127760; Provision Default Persona</h4>
      <div class="hint" style="margin-bottom:10px">Pull the published default persona &mdash; prompt, voice &amp; emoji &mdash; from the web and write it into this device. Use it to seed a freshly-flashed board (no emoji yet) or to load a newly published default. Replaces current settings; your browser needs internet.</div>
      <button type="button" id="provBtn" class="btn ghost sm" style="margin-top:0">&#11015;&#65039; Provision from web</button>
      <div id="provStatus" style="display:none;margin-top:12px;padding:9px;border:3px solid var(--ink);border-radius:8px;font-size:13px;font-weight:700;box-shadow:3px 3px 0 var(--ink)"></div>
    </div>

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

<script src="https://cdn.jsdelivr.net/npm/omggif@1.0.10/omggif.js" integrity="sha384-MyJfgL1BC/8IpGowcTSNInyyiYYLLUEEDACAIkOlzV68oYz87Pu5amdJWCO3NtFo" crossorigin="anonymous"></script>
<script src="https://cdn.jsdelivr.net/npm/jszip@3.10.1/dist/jszip.min.js" integrity="sha384-+mbV2IY1Zk/X1p/nWllGySJSUN8uMs+gUAN10Or95UBH0fpj6GfKgPmgC5EXieXG" crossorigin="anonymous"></script>
)=====" PORTAL_JS R"=====(<script>
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
  const langSel = $('language');
  langSel.innerHTML = '';
  (c.languages || []).forEach(l=>{
    const o = document.createElement('option');
    o.value = l.id; o.textContent = l.label;
    if (l.id === c.language) o.selected = true;
    langSel.appendChild(o);
  });

  // Volume
  $('volume').value = c.volume;
  $('volVal').textContent = c.volume + '%';

  // Audio output
  $(c.audioOut === 1 ? 'ao1' : 'ao0').checked = true;

  // Emoji card + default-persona provisioning (only when the display build is present)
  if (c.useDisplay){ $('emojiCard').style.display = 'flex'; $('provCard').style.display = ''; }

  // Admin: key status + behavior
  $('keyStatus').innerHTML = c.apiKeySet
    ? '<span style="color:var(--green)">&#9679; Custom key is set</span>'
    : '<span style="color:var(--mute)">&#9679; Using compiled default key</span>';
  $('persist').checked = !!c.persist;
  $('verbose').checked = !!c.verbose;
  $('showHidden').checked = !!c.showHidden;
  ioShowHidden = !!c.showHidden;

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

// ---- Import / Export profile (shared logic in portal_js.h -> window.VA) ----
function ioStatus(msg, kind){
  const s = $('ioStatus');
  s.textContent = msg;
  s.style.display = msg ? 'block' : 'none';
  s.style.background = kind==='err' ? 'var(--red)' : kind==='ok' ? 'var(--green)' : 'var(--yellow)';
  s.style.color = kind==='err' ? '#fff' : 'var(--ink)';
}
function ioSummary(res){
  let msg = res.applied.length ? ('Imported: ' + res.applied.join(', ')) : 'Nothing applied';
  if (res.skipped.length) msg += '  |  Skipped: ' + res.skipped.join('; ');
  ioStatus(msg, res.skipped.length ? 'info' : 'ok');
  if (res.applied.length) setTimeout(loadConfig, 400);  // refresh shown values
}
async function ioExport(){
  try{
    ioStatus('Building profile...', 'info');
    const zip = await VA.buildProfileZip(ioStatus);
    const blob = await zip.generateAsync({type:'blob'});
    VA.downloadBlob(blob, 'persona.zip');
    ioStatus('Exported persona.zip', 'ok');
  } catch(e){ ioStatus('Export failed: ' + e.message, 'err'); }
}
async function ioApply(zip){
  const res = await VA.applyProfileFromZip(zip, ioStatus);
  ioSummary(res);
}
// ---- Browse: curated persona catalog from the repo ----
let ioCatalogLoaded = false;
let ioShowHidden = false;   // admin "Show hidden personas" toggle (from /api/config)
function ioRenderCatalog(index){
  const root = $('ioRepoCatalog');
  root.innerHTML = '';
  const cats = (index && index.categories) || [];
  if (!cats.length){ root.innerHTML = '<div class="hint">No personas found in the gallery.</div>'; return; }
  cats.forEach(cat=>{
    // Hide personas flagged "hidden" unless the admin opted in.
    const people = (cat.personas || []).filter(p => ioShowHidden || !p.hidden);
    if (!people.length) return;   // skip a category emptied by filtering
    const sec = document.createElement('div'); sec.className='ioCat';
    const hdr = document.createElement('button'); hdr.type='button'; hdr.className='ioCatHdr';
    const n = people.length;
    hdr.innerHTML = '<span class="ioCaret">&#9656;</span>' + esc(cat.name || 'Personas') + '<span class="ioCatN">' + n + '</span>';
    const grid = document.createElement('div'); grid.className='ioGrid'; grid.style.display='none';
    for (const p of people){
      const tile = document.createElement('div'); tile.className='ioTile'; tile.title = 'Import ' + p.name;
      const src = p.thumb ? VA.repoFileUrl(p.dir, p.thumb) : '';
      tile.innerHTML = (src ? '<img loading="lazy" alt="" src="' + src + '">' : '') + '<div class="nm">' + esc(p.name) + '</div>';
      tile.addEventListener('click', ()=>ioImportPersona(p));
      grid.appendChild(tile);
    }
    hdr.addEventListener('click', ()=>{
      const willOpen = grid.style.display === 'none';
      // Accordion: close every section, then open the clicked one if it was closed.
      root.querySelectorAll('.ioGrid').forEach(g=>g.style.display='none');
      root.querySelectorAll('.ioCatHdr').forEach(h=>h.classList.remove('open'));
      if (willOpen){ grid.style.display='grid'; hdr.classList.add('open'); }
    });
    sec.appendChild(hdr); sec.appendChild(grid);
    root.appendChild(sec);
  });
}
async function ioLoadCatalog(){
  try{
    ioStatus('Loading gallery...', 'info');
    const index = await VA.fetchRepoIndex();
    ioRenderCatalog(index);
    ioCatalogLoaded = true;
    ioStatus('', '');
  } catch(e){ ioStatus('Gallery failed: ' + e.message, 'err'); }
}
async function ioImportPersona(p){
  if (!confirm('Import "' + p.name + '"? This replaces your persona text and emoji.')) return;
  try{ const res = await VA.importRepoPersona(p, ioStatus); ioSummary(res); }
  catch(e){ ioStatus('Import failed: ' + e.message, 'err'); }
}
$('ioBrowse').addEventListener('click', ()=>{
  const row = $('ioRepoRow');
  const show = row.style.display === 'none';
  row.style.display = show ? 'block' : 'none';
  if (show && !ioCatalogLoaded) ioLoadCatalog();
});

// ---- Admin: provision the canonical default persona from the repo ----
function provStatus(msg, kind){
  const s = $('provStatus');
  s.textContent = msg;
  s.style.display = msg ? 'block' : 'none';
  s.style.background = kind==='err' ? 'var(--red)' : kind==='ok' ? 'var(--green)' : 'var(--yellow)';
  s.style.color = kind==='err' ? '#fff' : 'var(--ink)';
}
$('provBtn').addEventListener('click', async ()=>{
  if (!confirm('Overwrite this device’s default persona (prompt, voice & emoji) from the web?\n\nThe current prompt and voice are replaced, and any custom emoji you uploaded are removed so the new defaults are what show on screen.')) return;
  const btn = $('provBtn');
  btn.disabled = true;
  try{
    const res = await VA.provisionDefaultPersona(provStatus);
    let msg = res.applied.length ? ('Provisioned: ' + res.applied.join(', ')) : 'No emoji written';
    if (res.skipped.length) msg += '  |  Skipped: ' + res.skipped.join('; ');
    provStatus(msg, res.skipped.length ? 'info' : 'ok');
    setTimeout(loadConfig, 500);   // refresh shown prompt/voice/volume
  } catch(e){ provStatus('Provision failed: ' + e.message, 'err'); }
  finally { btn.disabled = false; }
});

// ---- Admin: saved Wi-Fi networks ----
// The device never returns stored passwords, so this list is built purely from
// GET /api/wifi. Every mutation re-renders the list AND writes a plain-language
// line into #wfStatus, so it is always visible that an entry actually landed.
let wfApMode = false;

function wfSay(msg, kind){
  const s = $('wfStatus');
  s.innerHTML = msg;
  s.style.display = msg ? 'block' : 'none';
  s.style.background = kind==='err' ? 'var(--red)' : kind==='ok' ? 'var(--green)' : 'var(--yellow)';
  s.style.color = kind==='err' ? '#fff' : 'var(--ink)';
}
function wfWhen(epoch){
  if (!epoch) return '';
  const d = new Date(epoch*1000);
  const days = Math.floor((Date.now() - d.getTime())/86400000);
  if (days <= 0) return 'Last used today';
  if (days === 1) return 'Last used yesterday';
  if (days < 30) return 'Last used ' + days + ' days ago';
  return 'Last used ' + d.toLocaleDateString();
}
async function wfPost(path, ssid, pass){
  const body = new URLSearchParams();
  body.set('ssid', ssid);
  if (pass !== undefined) body.set('pass', pass);
  const r = await fetch(path, {method:'POST', body});
  if (!r.ok) throw new Error((await r.text()) || ('HTTP ' + r.status));
  return r.json();
}
async function wfLoad(){
  let d;
  try{
    d = await (await fetch('/api/wifi',{cache:'no-store'})).json();
  }catch(e){
    wfSay('Could not read saved networks: ' + e.message, 'err');
    return;
  }
  wfApMode = !!d.apMode;
  $('wfCount').textContent = d.count + ' of ' + d.max + ' saved';

  // A Connect request reboots the device, which destroys the HTTP response that
  // would have reported the outcome. The firmware remembers it instead; surface it
  // here so a target that could not be reached doesn't just fail silently.
  // Callers that follow wfLoad() with their own wfSay() intentionally override this.
  if (d.lastAttempt && !d.lastAttempt.ok){
    const on = (d.nets.find(n=>n.active) || {}).ssid;
    wfSay("Couldn't reach <b>" + esc(d.lastAttempt.ssid) + '</b> &mdash; '
          + (on ? 'still on <b>' + esc(on) + '</b>.' : 'not connected to anything.'), 'err');
  }
  const list = $('wfList');
  list.innerHTML = '';
  if (!d.nets.length){
    list.innerHTML = '<div class="hint" style="margin-top:8px">No networks saved yet. Add one below.</div>';
    return;
  }
  d.nets.forEach(n=>{
    const row = document.createElement('div');
    row.className = 'wfRow' + (n.active ? ' on' : '');
    const tag = n.active     ? '<span class="wfTag">Connected</span>'
              : n.neverUsed  ? '<span class="wfTag new">Never used</span>' : '';
    // neverUsed, not the timestamp, decides the wording: a network can have connected
    // on a boot where NTP never synced, leaving lastUsed at 0.
    const sub = n.active    ? 'In use now'
              : n.neverUsed ? 'Not connected yet'
              : (wfWhen(n.lastUsed) || 'Used before');
    row.innerHTML = '<div class="wfMeta"><div class="wfName">' + esc(n.ssid) + tag + '</div>'
                  + '<div class="wfSub">' + sub + '</div></div>';
    if (!n.active){
      const c = document.createElement('button');
      c.type='button'; c.className='wfBtn';
      c.textContent = wfApMode ? 'Connect & reboot' : 'Connect';
      c.addEventListener('click', ()=>wfConnect(n.ssid));
      row.appendChild(c);
    }
    const x = document.createElement('button');
    x.type='button'; x.className='wfBtn x'; x.textContent='Forget';
    x.addEventListener('click', ()=>wfForget(n.ssid, n.active));
    row.appendChild(x);
    list.appendChild(row);
  });
}
async function wfAddNetwork(){
  const ssid = $('wfSsid').value.trim();
  const pass = $('wfPass').value;
  if (!ssid){ wfSay('Enter a network name first.', 'err'); return; }
  const btn = $('wfAdd');
  btn.disabled = true;
  try{
    const r = await wfPost('/api/wifi/add', ssid, pass);
    let msg = r.updated
      ? 'Replaced the password for <b>' + esc(r.ssid) + '</b>'
      : 'Saved <b>' + esc(r.ssid) + '</b> &mdash; ' + r.count + ' of 6';
    if (r.evicted) msg += ', dropped <b>' + esc(r.evicted) + '</b> to make room';
    $('wfSsid').value = ''; $('wfPass').value = '';
    $('wfPick').style.display = 'none';
    await wfLoad();
    wfSay(msg, 'ok');
  }catch(e){ wfSay('Could not save: ' + e.message, 'err'); }
  finally { btn.disabled = false; }
}
async function wfForget(ssid, active){
  const warn = active
    ? 'Forget "' + ssid + '"? You are connected through it right now — the link stays up, but the device will not choose it again.'
    : 'Forget "' + ssid + '"?';
  if (!confirm(warn)) return;
  try{
    const r = await wfPost('/api/wifi/delete', ssid);
    await wfLoad();
    wfSay('Forgot <b>' + esc(r.ssid) + '</b> &mdash; ' + r.count + ' of 6 saved', 'ok');
  }catch(e){ wfSay('Could not forget it: ' + e.message, 'err'); }
}
async function wfConnect(ssid){
  if (!confirm('Switch to "' + ssid + '"? The device reboots and joins it, so this page will lose contact and the address may change.')) return;
  try{
    await wfPost('/api/wifi/connect', ssid);
    wfSay('Rebooting to join <b>' + esc(ssid) + '</b>. Reconnect to that network, then reload this page.', 'info');
  }catch(e){ wfSay('Could not switch: ' + e.message, 'err'); }
}
// Scan is asynchronous on the device (a blocking scan would stall the audio loop),
// so poll until it reports done.
async function wfRunScan(){
  const btn = $('wfScan');
  btn.disabled = true;
  wfSay('Looking for networks&hellip;', 'info');
  try{
    let d = null;
    for (let i = 0; i < 20; i++){
      const r = await fetch('/api/wifi/scan',{cache:'no-store'});
      if (!r.ok) throw new Error((await r.text()) || ('HTTP ' + r.status));
      d = await r.json();
      if (d.status === 'done') break;
      await new Promise(res=>setTimeout(res, 700));
    }
    if (!d || d.status !== 'done') throw new Error('scan timed out');
    const pick = $('wfPick');
    pick.innerHTML = '';
    if (!d.nets.length){
      pick.innerHTML = '<div class="hint" style="padding:9px">Nothing found. Try again, or type the name in by hand.</div>';
    } else {
      d.nets.forEach(n=>{
        const bars = n.rssi >= -55 ? '||||' : n.rssi >= -67 ? '|||' : n.rssi >= -78 ? '||' : '|';
        const row = document.createElement('div');
        row.className = 'wfPickRow';
        row.innerHTML = '<span class="wfBars">' + bars + '</span><span class="nm">' + esc(n.ssid)
                      + (n.saved ? '<span class="wfTag">Saved</span>' : '') + '</span>'
                      + (n.secure ? '<span>&#128274;</span>' : '');
        row.addEventListener('click', ()=>{
          $('wfSsid').value = n.ssid;
          pick.style.display = 'none';
          $('wfPass').focus();
        });
        pick.appendChild(row);
      });
    }
    pick.style.display = 'block';
    wfSay('', '');
  }catch(e){ wfSay('Scan failed: ' + e.message, 'err'); }
  finally { btn.disabled = false; }
}
$('wfScan').addEventListener('click', wfRunScan);
$('wfAdd').addEventListener('click', wfAddNetwork);
wfLoad();

$('ioExport').addEventListener('click', ioExport);
$('ioImportFile').addEventListener('click', ()=>$('ioFile').click());
$('ioFile').addEventListener('change', async ev=>{
  const f = ev.target.files[0]; ev.target.value=''; if (!f) return;
  try{ ioStatus('Reading file...', 'info'); await ioApply(await JSZip.loadAsync(f)); }
  catch(e){ ioStatus('Import failed: ' + e.message, 'err'); }
});
</script>
</body></html>)=====";

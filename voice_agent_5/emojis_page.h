// HTML+JS for the captive-portal emoji customization page (/emojis).
// Kept in a .h file because the Arduino auto-prototyper scans .ino files for
// C-style function declarations and chokes on the embedded JavaScript
// `async function ...` and `function ...` declarations inside the raw string.
// .h files are not scanned, so the JS is safe to keep verbatim here.

#pragma once

#include "theme_css.h"
#include "portal_js.h"

static const char EMOJIS_PAGE_HTML[] PROGMEM = R"PAGE(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Voice Agent - Emojis</title>
<meta name="viewport" content="width=device-width,initial-scale=1">)PAGE" THEME_CSS R"PAGE(<style>
.container{max-width:760px;margin:0 auto}
h2{position:relative;background:var(--red);color:#fff;border:4px solid var(--ink);border-radius:16px 16px 16px 3px;
   padding:14px 18px;font-weight:900;font-size:24px;text-shadow:2px 2px 0 var(--ink);box-shadow:7px 7px 0 var(--ink);margin:0 0 12px}
h3{color:var(--blue)}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin:14px 0}
.grid .row2{grid-column:span 1}
.row2-wrap{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-bottom:14px;max-width:580px}
.cell{border:4px solid var(--ink);border-radius:12px;padding:10px;text-align:center;background:#fff;box-shadow:5px 5px 0 var(--ink)}
.cell canvas{width:90px;height:90px;background:#000;border-radius:50%;display:block;margin:0 auto 6px;image-rendering:pixelated;border:3px solid var(--ink)}
.cell .name{font-weight:900;text-transform:capitalize;margin:4px 0;color:var(--blue)}
.cell .src{font-size:11px;color:var(--mute);font-weight:700;margin-bottom:8px}
.cell button{display:block;width:100%;margin:5px 0;padding:7px;font-size:13px;font-weight:900;text-transform:uppercase;
             border:3px solid var(--ink);border-radius:20px;cursor:pointer;box-shadow:3px 3px 0 var(--ink)}
.cell button:active{transform:translate(2px,2px);box-shadow:1px 1px 0 var(--ink)}
.btn-primary{background:var(--green);color:var(--ink)}
.btn-secondary{background:#fff;color:var(--ink)}
.btn-danger{background:var(--red);color:#fff}
.actions{display:flex;gap:12px;flex-wrap:wrap;margin:20px 0}
.actions button{flex:1;min-width:140px;padding:11px;font-size:14px;font-weight:900;text-transform:uppercase;
                border:4px solid var(--ink);border-radius:30px;cursor:pointer;box-shadow:5px 5px 0 var(--ink)}
.actions button:active{transform:translate(2px,2px);box-shadow:3px 3px 0 var(--ink)}
.actions .btn-secondary{background:var(--blue);color:#fff}
#status{padding:11px;border:3px solid var(--ink);border-radius:10px;margin:10px 0;display:none;font-weight:700;box-shadow:4px 4px 0 var(--ink)}
.status-ok{background:var(--green);color:var(--ink)}
.status-err{background:var(--red);color:#fff}
.status-info{background:var(--yellow);color:var(--ink)}
.help{font-size:13px;color:#444;font-weight:700;margin-top:8px}
.help a{color:var(--blue);font-weight:900}
nav{margin-bottom:10px}
nav a{color:var(--blue);text-decoration:none;font-weight:900}
</style></head><body>
<div class="container">
<nav><a href="/">&larr; Settings</a></nav>
<h2>Emojis</h2>
<p>Replace the on-device emoji animations with your own. GIF, PNG, JPG, or MP4 (H.264).</p>
<div id="status"></div>
<div id="grid-top" class="grid"></div>
<div id="grid-bottom" class="row2-wrap"></div>
<div class="actions">
<button id="btn-export" class="btn-secondary">Export pack</button>
<button id="btn-import" class="btn-secondary">Import pack</button>
<button id="btn-reset-all" class="btn-danger">Reset all to defaults</button>
</div>
<input type="file" id="file-import" accept=".zip" style="display:none">
<p class="help">Source images should be square; the display is 240x240 round and corners are clipped. <a href="https://github.com/iollama/Voice-Assistant-5/blob/main/voice_agent_5/docs/emoji-customization.md" target="_blank">How to prepare emojis</a> for developers. (Everyone else can simply upload from here) </p>
<div class="footer">developed by <a href="https://www.iollama.com/" target="_blank" rel="noopener">iollama</a></div>
</div>
<script src="https://cdn.jsdelivr.net/npm/omggif@1.0.10/omggif.js" integrity="sha384-MyJfgL1BC/8IpGowcTSNInyyiYYLLUEEDACAIkOlzV68oYz87Pu5amdJWCO3NtFo" crossorigin="anonymous"></script>
<script src="https://cdn.jsdelivr.net/npm/jszip@3.10.1/dist/jszip.min.js" integrity="sha384-+mbV2IY1Zk/X1p/nWllGySJSUN8uMs+gUAN10Or95UBH0fpj6GfKgPmgC5EXieXG" crossorigin="anonymous"></script>
)PAGE" PORTAL_JS R"PAGE(<script>
const EMOTIONS_TOP = ["neutral","happy","excited","empathetic"];
const EMOTIONS_BOTTOM = ["confused","concerned","thinking"];
const ALL_EMOTIONS = EMOTIONS_TOP.concat(EMOTIONS_BOTTOM);
const { FRAME_PX, FRAME_BYTES, MAX_FRAMES, decodeFile, uploadFrames, rgb565_le_to_rgba } = VA;

const $ = id => document.getElementById(id);
function setStatus(msg, kind){
  const s = $('status');
  s.textContent = msg;
  s.className = 'status-' + (kind||'info');
  s.style.display = msg ? 'block' : 'none';
}

async function paintThumbnail(emotion){
  const c = $('thumb-'+emotion);
  if (!c) return;
  try{
    const r = await fetch('/api/emoji/'+emotion+'/0.bin', {cache:'no-store'});
    if (!r.ok) return;
    const bytes = new Uint8Array(await r.arrayBuffer());
    if (bytes.length !== FRAME_BYTES) return;
    const big = document.createElement('canvas');
    big.width = FRAME_PX; big.height = FRAME_PX;
    const bctx = big.getContext('2d');
    const img = bctx.createImageData(FRAME_PX, FRAME_PX);
    img.data.set(rgb565_le_to_rgba(bytes));
    bctx.putImageData(img, 0, 0);
    const ctx = c.getContext('2d');
    ctx.clearRect(0,0,c.width,c.height);
    ctx.drawImage(big, 0, 0, c.width, c.height);
  } catch(e){ console.warn(e); }
}

function buildCell(emotion, manifest){
  const m = manifest[emotion] || {count:0, source:'default'};
  const div = document.createElement('div'); div.className='cell';
  div.innerHTML =
    '<canvas id="thumb-'+emotion+'" width="90" height="90"></canvas>'+
    '<div class="name">'+emotion+'</div>'+
    '<div class="src">'+m.source+' &middot; '+m.count+' frame'+(m.count===1?'':'s')+'</div>'+
    '<button class="btn-primary" data-replace="'+emotion+'">Replace</button>'+
    '<button class="btn-secondary" data-reset="'+emotion+'">Reset</button>'+
    '<input type="file" accept=".gif,.png,.jpg,.jpeg,.mp4,image/gif,image/png,image/jpeg,video/mp4" data-file="'+emotion+'" style="display:none">';
  return div;
}

async function refreshGrid(){
  setStatus('Loading...', 'info');
  let manifest = {};
  try{
    const r = await fetch('/api/emoji/manifest', {cache:'no-store'});
    manifest = await r.json();
  } catch(e){
    setStatus('Failed to load manifest: '+e.message, 'err');
    return;
  }
  const top = $('grid-top'); top.innerHTML='';
  const bot = $('grid-bottom'); bot.innerHTML='';
  for (const e of EMOTIONS_TOP)    top.appendChild(buildCell(e, manifest));
  for (const e of EMOTIONS_BOTTOM) bot.appendChild(buildCell(e, manifest));
  for (const e of ALL_EMOTIONS) paintThumbnail(e);

  document.querySelectorAll('[data-replace]').forEach(b=>{
    b.addEventListener('click', ()=>{
      const e = b.dataset.replace;
      document.querySelector('[data-file="'+e+'"]').click();
    });
  });
  document.querySelectorAll('[data-file]').forEach(input=>{
    input.addEventListener('change', async ev=>{
      const file = ev.target.files[0]; if (!file) return;
      const emotion = ev.target.dataset.file;
      ev.target.value='';
      await replaceOne(emotion, file);
    });
  });
  document.querySelectorAll('[data-reset]').forEach(b=>{
    b.addEventListener('click', async ()=>{
      const e = b.dataset.reset;
      if (!confirm('Reset '+e+' to default?')) return;
      setStatus('Resetting '+e+'...', 'info');
      const r = await fetch('/api/emoji/'+e+'/reset', {method:'POST'});
      if (r.ok){ setStatus('Reset '+e, 'ok'); refreshGrid(); }
      else setStatus('Reset failed: '+await r.text(), 'err');
    });
  });
  setStatus('', '');
}

async function replaceOne(emotion, file){
  try{
    setStatus('Converting '+file.name+'...', 'info');
    const frames = await decodeFile(file);
    setStatus('Uploading '+frames.length+' frame'+(frames.length===1?'':'s')+' to '+emotion+'...', 'info');
    const res = await uploadFrames(emotion, frames);
    setStatus('Replaced '+emotion+' ('+res.count+' frame'+(res.count===1?'':'s')+')', 'ok');
    refreshGrid();
  } catch(e){
    setStatus('Failed: '+e.message, 'err');
  }
}

async function exportPack(){
  try{
    setStatus('Building pack...', 'info');
    const zip = new JSZip();
    const n = await VA.addEmojiToZip(zip, {customOnly:true, statusCb:setStatus});
    if (n === 0){ setStatus('No custom emojis to export', 'info'); return; }
    const blob = await zip.generateAsync({type:'blob'});
    VA.downloadBlob(blob, 'emoji-pack.zip');
    setStatus('Exported '+n+' custom emoji'+(n===1?'':'s'), 'ok');
  } catch(e){
    setStatus('Export failed: '+e.message, 'err');
  }
}

// Emoji page imports emoji ONLY -- any persona.md / settings.json in the zip is
// ignored here (the main settings page is where a full profile is imported).
async function importPack(file){
  try{
    setStatus('Reading pack...', 'info');
    const zip = await JSZip.loadAsync(file);
    const res = await VA.applyEmojiFromZip(zip, setStatus);
    if (!res.found) throw new Error('Pack contains no recognizable emoji files');
    let msg = 'Imported '+res.applied.length+' emoji';
    if (res.skipped.length) msg += ', skipped '+res.skipped.length+' ('+res.skipped.map(s=>s.name).join(', ')+')';
    setStatus(msg, res.skipped.length ? 'info' : 'ok');
    refreshGrid();
  } catch(e){
    setStatus('Import failed: '+e.message, 'err');
  }
}

$('btn-export').addEventListener('click', exportPack);
$('btn-import').addEventListener('click', ()=>$('file-import').click());
$('file-import').addEventListener('change', ev=>{
  const f = ev.target.files[0]; ev.target.value=''; if (f) importPack(f);
});
$('btn-reset-all').addEventListener('click', async ()=>{
  if (!confirm('Reset every emoji to its shipped default?')) return;
  setStatus('Resetting all...', 'info');
  const r = await fetch('/api/emoji/reset-all', {method:'POST'});
  if (r.ok){ setStatus('Reset all', 'ok'); refreshGrid(); }
  else setStatus('Reset all failed: '+await r.text(), 'err');
});

refreshGrid();
</script>
</body></html>)PAGE";

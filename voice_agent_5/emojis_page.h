// HTML+JS for the captive-portal emoji customization page (/emojis).
// Kept in a .h file because the Arduino auto-prototyper scans .ino files for
// C-style function declarations and chokes on the embedded JavaScript
// `async function ...` and `function ...` declarations inside the raw string.
// .h files are not scanned, so the JS is safe to keep verbatim here.

#pragma once

static const char EMOJIS_PAGE_HTML[] PROGMEM = R"PAGE(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><title>Voice Agent - Emojis</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:Arial,sans-serif;background:#f4f4f4;margin:0;padding:20px;color:#222}
.container{background:#fff;max-width:760px;margin:auto;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,.1)}
h2,h3{color:#333}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin:14px 0}
.grid .row2{grid-column:span 1}
.row2-wrap{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:14px;max-width:570px}
.cell{border:1px solid #ddd;border-radius:8px;padding:8px;text-align:center;background:#fafafa}
.cell canvas{width:90px;height:90px;background:#000;border-radius:50%;display:block;margin:0 auto 4px;image-rendering:pixelated}
.cell .name{font-weight:bold;text-transform:capitalize;margin:4px 0}
.cell .src{font-size:11px;color:#888;margin-bottom:6px}
.cell button{display:block;width:100%;margin:3px 0;padding:6px;font-size:13px;border:none;border-radius:4px;cursor:pointer}
.btn-primary{background:#007bff;color:#fff}
.btn-primary:hover{background:#0056b3}
.btn-secondary{background:#6c757d;color:#fff}
.btn-secondary:hover{background:#5a6268}
.btn-danger{background:#dc3545;color:#fff}
.btn-danger:hover{background:#c82333}
.actions{display:flex;gap:10px;flex-wrap:wrap;margin:20px 0}
.actions button{flex:1;min-width:140px;padding:10px;font-size:14px;border:none;border-radius:4px;cursor:pointer}
#status{padding:10px;border-radius:4px;margin:10px 0;display:none}
.status-ok{background:#d4edda;color:#155724}
.status-err{background:#f8d7da;color:#721c24}
.status-info{background:#d1ecf1;color:#0c5460}
.help{font-size:13px;color:#555;margin-top:8px}
.help a{color:#007bff}
nav{margin-bottom:10px}
nav a{color:#007bff;text-decoration:none}
</style></head><body>
<div class="container">
<nav><a href="/">&larr; Settings</a></nav>
<h2>Emojis</h2>
<p>Replace the on-device emoji animations with your own. GIF (animated, up to 8 frames), PNG, or JPG. Conversion happens in your browser - no Python required.</p>
<div id="status"></div>
<div id="grid-top" class="grid"></div>
<div id="grid-bottom" class="row2-wrap"></div>
<div class="actions">
<button id="btn-export" class="btn-secondary">Export pack</button>
<button id="btn-import" class="btn-secondary">Import pack</button>
<button id="btn-reset-all" class="btn-danger">Reset all to defaults</button>
</div>
<input type="file" id="file-import" accept=".zip" style="display:none">
<p class="help">Source images should be square; the display is 240x240 round and corners are clipped. <a href="https://github.com/uditir/05_voice_agent_5/blob/main/voice_agent_5/docs/emoji-customization.md" target="_blank">How to prepare emojis</a></p>
</div>
<script src="https://cdn.jsdelivr.net/npm/omggif@1.0.10/omggif.js" integrity="sha384-MyJfgL1BC/8IpGowcTSNInyyiYYLLUEEDACAIkOlzV68oYz87Pu5amdJWCO3NtFo" crossorigin="anonymous"></script>
<script src="https://cdn.jsdelivr.net/npm/jszip@3.10.1/dist/jszip.min.js" integrity="sha384-+mbV2IY1Zk/X1p/nWllGySJSUN8uMs+gUAN10Or95UBH0fpj6GfKgPmgC5EXieXG" crossorigin="anonymous"></script>
<script>
const EMOTIONS_TOP = ["neutral","happy","excited","empathetic"];
const EMOTIONS_BOTTOM = ["confused","concerned","thinking"];
const ALL_EMOTIONS = EMOTIONS_TOP.concat(EMOTIONS_BOTTOM);
const FRAME_PX = 150;
const FRAME_BYTES = FRAME_PX*FRAME_PX*2;
const MAX_FRAMES = 8;

const $ = id => document.getElementById(id);
function setStatus(msg, kind){
  const s = $('status');
  s.textContent = msg;
  s.className = 'status-' + (kind||'info');
  s.style.display = msg ? 'block' : 'none';
}

function rgba_to_rgb565_le(rgba){
  const out = new Uint8Array(FRAME_BYTES);
  for (let i=0,j=0; i<rgba.length; i+=4, j+=2){
    const r=rgba[i], g=rgba[i+1], b=rgba[i+2];
    const v=((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
    out[j]=v&0xFF; out[j+1]=v>>8;
  }
  return out;
}
function rgb565_le_to_rgba(bytes){
  const out = new Uint8ClampedArray(FRAME_PX*FRAME_PX*4);
  for (let i=0,j=0; i<bytes.length; i+=2, j+=4){
    const v = bytes[i]|(bytes[i+1]<<8);
    const r5=(v>>11)&0x1F, g6=(v>>5)&0x3F, b5=v&0x1F;
    out[j]   = (r5<<3)|(r5>>2);
    out[j+1] = (g6<<2)|(g6>>4);
    out[j+2] = (b5<<3)|(b5>>2);
    out[j+3] = 255;
  }
  return out;
}

function drawCover(ctx, src, sw, sh){
  ctx.fillStyle='#000'; ctx.fillRect(0,0,FRAME_PX,FRAME_PX);
  const scale = Math.max(FRAME_PX/sw, FRAME_PX/sh);
  const dw = sw*scale, dh = sh*scale;
  ctx.drawImage(src, (FRAME_PX-dw)/2, (FRAME_PX-dh)/2, dw, dh);
}

async function decodeFile(file){
  const buf = new Uint8Array(await file.arrayBuffer());
  const isGif = buf.length>3 && buf[0]==0x47 && buf[1]==0x49 && buf[2]==0x46;
  const isPng = buf.length>3 && buf[0]==0x89 && buf[1]==0x50;
  const isJpg = buf.length>2 && buf[0]==0xFF && buf[1]==0xD8;
  if (!isGif && !isPng && !isJpg) throw new Error('Unsupported file type (use GIF, PNG, or JPG)');

  const canvas = document.createElement('canvas');
  canvas.width = FRAME_PX; canvas.height = FRAME_PX;
  const ctx = canvas.getContext('2d');

  if (isGif){
    const reader = new GifReader(buf);
    const total = reader.numFrames();
    const want = new Set();
    if (total <= MAX_FRAMES) for (let i=0;i<total;i++) want.add(i);
    else for (let i=0;i<MAX_FRAMES;i++) want.add(Math.floor(i*total/MAX_FRAMES));
    const lw = reader.width, lh = reader.height;
    const pixels = new Uint8Array(lw*lh*4);
    const work = document.createElement('canvas'); work.width=lw; work.height=lh;
    const wctx = work.getContext('2d');
    const out = [];
    for (let i=0; i<total; i++){
      reader.decodeAndBlitFrameRGBA(i, pixels);
      if (!want.has(i)) continue;
      const id = new ImageData(new Uint8ClampedArray(pixels), lw, lh);
      wctx.putImageData(id, 0, 0);
      drawCover(ctx, work, lw, lh);
      out.push(rgba_to_rgb565_le(ctx.getImageData(0,0,FRAME_PX,FRAME_PX).data));
    }
    return out;
  }

  const img = new Image();
  await new Promise((res,rej)=>{ img.onload=res; img.onerror=()=>rej(new Error('Decode failed')); img.src=URL.createObjectURL(file); });
  drawCover(ctx, img, img.naturalWidth, img.naturalHeight);
  return [rgba_to_rgb565_le(ctx.getImageData(0,0,FRAME_PX,FRAME_PX).data)];
}

async function uploadFrames(emotion, frames){
  const body = new Uint8Array(frames.length * FRAME_BYTES);
  for (let i=0; i<frames.length; i++) body.set(frames[i], i*FRAME_BYTES);
  const fd = new FormData();
  fd.append('frames', new Blob([body], {type:'application/octet-stream'}), 'frames.bin');
  const r = await fetch('/api/emoji/'+emotion, {method:'POST', body:fd});
  if (!r.ok){
    const t = await r.text();
    throw new Error('Upload failed: ' + r.status + ' ' + t);
  }
  return r.json();
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
    '<input type="file" accept=".gif,.png,.jpg,.jpeg,image/gif,image/png,image/jpeg" data-file="'+emotion+'" style="display:none">';
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
    const manifest = await (await fetch('/api/emoji/manifest', {cache:'no-store'})).json();
    const zip = new JSZip();
    const customManifest = {};
    for (const e of ALL_EMOTIONS){
      const m = manifest[e];
      if (!m || m.source !== 'custom') continue;
      customManifest[e] = {count: m.count};
      for (let i=0; i<m.count; i++){
        const r = await fetch('/api/emoji/'+e+'/'+i+'.bin', {cache:'no-store'});
        const buf = await r.arrayBuffer();
        zip.file(e+'_'+i+'.bin', buf);
      }
    }
    if (Object.keys(customManifest).length === 0){
      setStatus('No custom emojis to export', 'info'); return;
    }
    zip.file('manifest.json', JSON.stringify(customManifest, null, 2));
    const blob = await zip.generateAsync({type:'blob'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'emoji-pack.zip';
    a.click();
    setStatus('Exported '+Object.keys(customManifest).length+' custom emoji'+(Object.keys(customManifest).length===1?'':'s'), 'ok');
  } catch(e){
    setStatus('Export failed: '+e.message, 'err');
  }
}

async function importPack(file){
  try{
    setStatus('Reading pack...', 'info');
    const zip = await JSZip.loadAsync(file);
    const hasBin = Object.keys(zip.files).some(n=>n.endsWith('.bin'));
    const hasSrc = ALL_EMOTIONS.some(e=>['gif','png','jpg','jpeg'].some(ext=>zip.files[e+'.'+ext]));
    if (!hasBin && !hasSrc) throw new Error('Pack contains no recognizable emoji files');

    if (hasBin){
      const manifestEntry = zip.files['manifest.json'];
      if (!manifestEntry) throw new Error('Missing manifest.json in .bin pack');
      const manifest = JSON.parse(await manifestEntry.async('string'));
      for (const e of Object.keys(manifest)){
        if (!ALL_EMOTIONS.includes(e)) continue;
        const count = manifest[e].count;
        const parts = [];
        for (let i=0; i<count; i++){
          const entry = zip.files[e+'_'+i+'.bin'];
          if (!entry) throw new Error('Missing '+e+'_'+i+'.bin');
          const bytes = new Uint8Array(await entry.async('uint8array'));
          if (bytes.length !== FRAME_BYTES) throw new Error('Bad frame size for '+e+'_'+i);
          parts.push(bytes);
        }
        setStatus('Importing '+e+'...', 'info');
        await uploadFrames(e, parts);
      }
    } else {
      for (const e of ALL_EMOTIONS){
        let entry = null;
        for (const ext of ['gif','png','jpg','jpeg']){
          if (zip.files[e+'.'+ext]) { entry = zip.files[e+'.'+ext]; break; }
        }
        if (!entry) continue;
        const blob = await entry.async('blob');
        setStatus('Converting & importing '+e+'...', 'info');
        const frames = await decodeFile(new File([blob], entry.name));
        await uploadFrames(e, frames);
      }
    }
    setStatus('Import done', 'ok');
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

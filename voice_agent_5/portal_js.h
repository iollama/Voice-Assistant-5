// Shared browser JavaScript for the captive-portal pages (/ and /emojis).
// PORTAL_JS is a raw-string-literal macro that root_page.h and emojis_page.h
// concatenate inline (adjacent string literals), exactly like THEME_CSS in
// theme_css.h -- so each page stays a single PROGMEM blob served by send_P,
// with no extra HTTP round-trip and no new endpoint.
//
// It exposes a small `window.VA` namespace of reusable helpers (RGB565 frame
// conversion, image/GIF/MP4 decoding, emoji upload, and the emoji/profile ZIP
// build + apply logic) so the two pages share one codebase. Page-specific glue
// (status element, button wiring) stays in each page's own <script>.
//
// Requires JSZip (and, for source GIF import, omggif) to be loaded first; both
// pages pull those from the jsdelivr CDN, so import/export needs the browser to
// be online -- same constraint as before this file existed.
//
// Kept in a .h (not .ino) for the same reason as theme_css.h / the page files:
// the Arduino auto-prototyper scans .ino files and chokes on embedded JS.

#pragma once

#define PORTAL_JS R"JS(<script>
(function(){
  const VA = (window.VA = window.VA || {});

  const FRAME_PX = 150;
  const FRAME_BYTES = FRAME_PX*FRAME_PX*2;   // 45000, RGB565 LE
  const MAX_FRAMES = 8;
  const PERSONA_MAX = 12288;
  const ALL_EMOTIONS = ["neutral","happy","excited","empathetic","confused","concerned","thinking"];
  const SRC_EXTS = ["gif","png","jpg","jpeg","mp4"];

  // ---- "Browse" persona catalog source -------------------------------------
  // Personas are stored loose (persona.md + emotion images) in the repo under
  // personas/, with a generated personas/index.json catalog. Change REPO/BRANCH
  // to test against your own fork/branch; lock to iollama/Voice-Assistant-5 +
  // main before a public release. raw.githubusercontent.com sends CORS '*'.
  const REPO   = "iollama/Voice-Assistant-5";   // owner/repo
  const BRANCH = "main";                          // branch or tag
  const REPO_BASE = "https://raw.githubusercontent.com/" + REPO + "/" + BRANCH + "/personas/";

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
    // MP4 / ISO base media: 'ftyp' box at offset 4.
    const isMp4 = buf.length>11 && buf[4]==0x66 && buf[5]==0x74 && buf[6]==0x79 && buf[7]==0x70;
    if (!isGif && !isPng && !isJpg && !isMp4) throw new Error('Unsupported file type (use GIF, PNG, JPG, or MP4)');

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

    if (isMp4){
      // Decode via a <video> element: the browser handles H.264 natively, no
      // transcoding. Seek to MAX_FRAMES evenly-spaced timestamps and grab a still
      // at each through the same drawCover -> RGB565 path used by GIF/image.
      const video = document.createElement('video');
      video.muted = true; video.playsInline = true; video.preload = 'auto';
      const url = URL.createObjectURL(file);
      try {
        await new Promise((res,rej)=>{
          video.onloadedmetadata = res;
          video.onerror = ()=>rej(new Error('Decode failed (unsupported codec? use H.264/MP4)'));
          video.src = url;
        });
        const dur = video.duration, vw = video.videoWidth, vh = video.videoHeight;
        if (!isFinite(dur) || dur<=0 || !vw || !vh) throw new Error('Decode failed (unsupported codec? use H.264/MP4)');
        const out = [];
        for (let i=0; i<MAX_FRAMES; i++){
          const t = Math.min(dur * (i+0.5)/MAX_FRAMES, Math.max(0, dur-0.01));
          await new Promise((res,rej)=>{
            // Resolve on seeked, or a fallback timer so a browser that doesn't
            // fire seeked (e.g. an already-current timestamp) can't hang the UI.
            const done = ()=>{ clearTimeout(timer); res(); };
            const timer = setTimeout(done, 1500);
            video.onseeked = done;
            video.onerror = ()=>{ clearTimeout(timer); rej(new Error('Decode failed')); };
            video.currentTime = t;
          });
          drawCover(ctx, video, vw, vh);
          out.push(rgba_to_rgb565_le(ctx.getImageData(0,0,FRAME_PX,FRAME_PX).data));
        }
        return out;
      } finally {
        URL.revokeObjectURL(url);
      }
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

  // Pack emoji frames into an existing JSZip. customOnly=true emits only the
  // emotions the user replaced (emoji-pack.zip); customOnly=false emits all 7
  // effective emotions (profile zip). Always writes manifest.json describing the
  // emotions it added. Returns the number of emotions packed.
  async function addEmojiToZip(zip, opts){
    opts = opts || {};
    const status = opts.statusCb || function(){};
    const manifest = await (await fetch('/api/emoji/manifest', {cache:'no-store'})).json();
    const outManifest = {};
    let n = 0;
    for (const e of ALL_EMOTIONS){
      const m = manifest[e];
      if (!m) continue;
      if (opts.customOnly && m.source !== 'custom') continue;
      outManifest[e] = {count: m.count};
      for (let i=0; i<m.count; i++){
        status('Packing '+e+' '+(i+1)+'/'+m.count+'...', 'info');
        const r = await fetch('/api/emoji/'+e+'/'+i+'.bin', {cache:'no-store'});
        if (!r.ok) throw new Error('Failed to read '+e+'_'+i+'.bin');
        zip.file(e+'_'+i+'.bin', await r.arrayBuffer());
      }
      n++;
    }
    zip.file('manifest.json', JSON.stringify(outManifest, null, 2));
    return n;
  }

  function zipHasEmoji(zip){
    const hasBin = Object.keys(zip.files).some(name=>name.endsWith('.bin'));
    const hasSrc = ALL_EMOTIONS.some(e=>SRC_EXTS.some(ext=>zip.files[e+'.'+ext]));
    return hasBin || hasSrc;
  }

  // Apply emoji from a generic file map (name -> entry, where entry.async('string'
  // |'uint8array'|'blob') yields the contents -- this is exactly the shape of a
  // JSZip `.files` object, and also what `memFiles()` produces from fetched URLs).
  // Accepts a .bin pack (manifest.json + <emotion>_<n>.bin) or source images/videos
  // named <emotion>.<ext>. Resilient: a bad/missing frame skips only that emotion.
  // Returns {found, applied:[names], skipped:[{name,reason}]}.
  async function applyEmoji(files, statusCb){
    const status = statusCb || function(){};
    const applied = [], skipped = [];
    const hasBin = Object.keys(files).some(name=>name.endsWith('.bin'));
    const hasSrc = ALL_EMOTIONS.some(e=>SRC_EXTS.some(ext=>files[e+'.'+ext]));
    if (!hasBin && !hasSrc) return {found:false, applied, skipped};

    if (hasBin){
      const me = files['manifest.json'];
      if (!me){ skipped.push({name:'(all)', reason:'missing manifest.json in .bin pack'}); return {found:true, applied, skipped}; }
      let manifest;
      try { manifest = JSON.parse(await me.async('string')); }
      catch(e){ skipped.push({name:'(all)', reason:'bad manifest.json'}); return {found:true, applied, skipped}; }
      for (const e of Object.keys(manifest)){
        if (!ALL_EMOTIONS.includes(e)){ skipped.push({name:e, reason:'unknown emotion'}); continue; }
        try {
          const count = manifest[e].count;
          const parts = [];
          for (let i=0; i<count; i++){
            const entry = files[e+'_'+i+'.bin'];
            if (!entry) throw new Error('missing '+e+'_'+i+'.bin');
            const bytes = new Uint8Array(await entry.async('uint8array'));
            if (bytes.length !== FRAME_BYTES) throw new Error('bad frame size');
            parts.push(bytes);
          }
          status('Importing '+e+'...', 'info');
          await uploadFrames(e, parts);
          applied.push(e);
        } catch(err){ skipped.push({name:e, reason:err.message}); }
      }
    } else {
      for (const e of ALL_EMOTIONS){
        let entry = null, ename = null;
        for (const ext of SRC_EXTS){ if (files[e+'.'+ext]){ entry = files[e+'.'+ext]; ename = e+'.'+ext; break; } }
        if (!entry) continue;
        try {
          const blob = await entry.async('blob');
          status('Converting & importing '+e+'...', 'info');
          const frames = await decodeFile(new File([blob], ename));
          await uploadFrames(e, frames);
          applied.push(e);
        } catch(err){ skipped.push({name:e, reason:err.message}); }
      }
    }
    return {found:true, applied, skipped};
  }
  const applyEmojiFromZip = (zip, statusCb) => applyEmoji(zip.files, statusCb);

  // Build the full device-profile zip: persona.md + settings.json (volume+voice,
  // versioned) + all-7 emoji frames. Returns the populated JSZip.
  async function buildProfileZip(statusCb){
    const status = statusCb || function(){};
    status('Reading settings...', 'info');
    const cfg = await (await fetch('/api/config', {cache:'no-store'})).json();
    const zip = new JSZip();
    zip.file('persona.md', cfg.sysPrompt || '');
    zip.file('settings.json', JSON.stringify({version:1, volume:cfg.volume, voice:cfg.voice, language:cfg.language}, null, 2));
    await addEmojiToZip(zip, {customOnly:false, statusCb:status});
    return zip;
  }

  async function postForm(path, fields){
    const body = new URLSearchParams();
    for (const k of Object.keys(fields)) body.append(k, fields[k]);
    const r = await fetch(path, {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body: body.toString()});
    // The save handlers reply 302 -> "/"; fetch follows it, so r.ok reflects the
    // followed GET. Treat any non-error final response as success.
    if (!r.ok) throw new Error('save failed ' + r.status);
  }

  // Apply whatever a file map contains. Merges against current /api/config so an
  // absent piece is never wiped (critical: /saveAgent overwrites sysPrompt
  // unconditionally). Values present-but-not-importable are skipped, not fatal.
  // `files` is a name -> entry map (JSZip `.files` or memFiles()); no zip needed.
  // Returns {applied:[...], skipped:[...]} for a status summary.
  async function applyProfile(files, statusCb){
    const status = statusCb || function(){};
    const applied = [], skipped = [];

    let cfg = {};
    let cfgOk = false;
    try { cfg = await (await fetch('/api/config', {cache:'no-store'})).json(); cfgOk = true; } catch(e){}
    const haveBasePrompt = cfgOk && typeof cfg.sysPrompt === 'string';

    // ---- persona.md ----
    let sysPrompt = haveBasePrompt ? cfg.sysPrompt : '';
    let personaPresent = false;
    const personaEntry = files['persona.md'];
    if (personaEntry){
      const txt = await personaEntry.async('string');
      if (txt.length > PERSONA_MAX) skipped.push('persona (too long, >'+PERSONA_MAX+' chars)');
      else { sysPrompt = txt; personaPresent = true; }
    }

    // ---- settings.json (volume + voice + language; unknown keys ignored) ----
    let voice = cfgOk ? (cfg.voice || '') : '';
    let voicePresent = false;
    let language = cfgOk ? (cfg.language || '') : '';
    let languagePresent = false;
    let volumeVal = null, volumePresent = false;
    const settingsEntry = files['settings.json'];
    if (settingsEntry){
      let s = null;
      try { s = JSON.parse(await settingsEntry.async('string')); }
      catch(e){ skipped.push('settings.json (invalid JSON)'); }
      if (s){
        if ('voice' in s){
          if (typeof s.voice === 'string' && cfgOk && (cfg.voices||[]).some(v=>v.id===s.voice)){ voice = s.voice; voicePresent = true; }
          else skipped.push('voice ('+s.voice+' not available)');
        }
        if ('language' in s){
          if (typeof s.language === 'string' && cfgOk && (cfg.languages||[]).some(l=>l.id===s.language)){ language = s.language; languagePresent = true; }
          else skipped.push('language ('+s.language+' not available)');
        }
        if ('volume' in s){
          const v = parseInt(s.volume, 10);
          if (Number.isFinite(v)){ volumeVal = Math.max(0, Math.min(100, v)); volumePresent = true; }
          else skipped.push('volume (not a number)');
        }
      }
    }

    // ---- apply persona + voice + language via /saveAgent (merged) ----
    if (personaPresent || voicePresent || languagePresent){
      if (!personaPresent && !haveBasePrompt){
        // Can't read current persona to merge -> refuse rather than wipe it.
        if (voicePresent) skipped.push('voice (could not read current persona to merge safely)');
        if (languagePresent) skipped.push('language (could not read current persona to merge safely)');
      } else {
        try {
          status('Applying persona/voice/language...', 'info');
          await postForm('/saveAgent', {sysPrompt: sysPrompt, voice: voice, language: language});
          if (personaPresent) applied.push('persona');
          if (voicePresent) applied.push('voice');
          if (languagePresent) applied.push('language');
        } catch(e){ skipped.push('persona/voice/language ('+e.message+')'); }
      }
    }

    // ---- apply volume via /saveVolume ----
    if (volumePresent){
      try {
        status('Applying volume...', 'info');
        await postForm('/saveVolume', {volume: String(volumeVal)});
        applied.push('volume');
      } catch(e){ skipped.push('volume ('+e.message+')'); }
    }

    // ---- apply emoji ----
    status('Importing emoji...', 'info');
    const em = await applyEmoji(files, status);
    if (em.found){
      em.applied.forEach(e=>applied.push('emoji:'+e));
      em.skipped.forEach(s=>skipped.push('emoji:'+s.name+' ('+s.reason+')'));
    }

    return {applied, skipped};
  }
  const applyProfileFromZip = (zip, statusCb) => applyProfile(zip.files, statusCb);

  // Build a file map (name -> entry with .async) from already-fetched blobs, so
  // the apply/decoding path is identical to a JSZip without any zipping.
  function memFiles(map){
    const out = {};
    for (const name of Object.keys(map)){
      const blob = map[name];
      out[name] = {
        async: async (type)=>{
          if (type === 'string') return await blob.text();
          if (type === 'uint8array') return new Uint8Array(await blob.arrayBuffer());
          return blob;  // 'blob' (or default)
        }
      };
    }
    return out;
  }

  async function fetchZipFromUrl(url){
    let resp;
    try { resp = await fetch(url, {cache:'no-store'}); }
    catch(e){ throw new Error("Couldn't fetch URL (blocked by CORS or no internet). Use a direct, CORS-enabled link, e.g. a GitHub raw/release URL."); }
    if (!resp.ok) throw new Error('URL returned HTTP ' + resp.status);
    const blob = await resp.blob();
    return await JSZip.loadAsync(blob);
  }

  function downloadBlob(blob, filename){
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = filename;
    a.click();
    setTimeout(()=>URL.revokeObjectURL(a.href), 1000);
  }

  // ---- "Browse" repository catalog -----------------------------------------
  // Fetch the curated persona catalog (personas/index.json) from the repo.
  async function fetchRepoIndex(){
    let resp;
    try { resp = await fetch(REPO_BASE + 'index.json', {cache:'no-store'}); }
    catch(e){ throw new Error("Couldn't reach the persona repository (offline, or CORS blocked). The browser needs internet access."); }
    if (!resp.ok) throw new Error('Catalog fetch failed: HTTP ' + resp.status);
    return await resp.json();
  }

  // Full URL to a file inside a catalog persona's folder.
  function repoFileUrl(dir, file){ return REPO_BASE + dir.split('/').map(encodeURIComponent).join('/') + '/' + encodeURIComponent(file); }

  // Import one persona straight from the repo (loose files, never zipped):
  // fetch persona.md + each listed emotion image + optional settings.json into a
  // file map, then run the same apply path as a profile import.
  async function importRepoPersona(entry, statusCb){
    const status = statusCb || function(){};
    const map = {};
    async function grab(file, key){
      status('Downloading ' + file + '...', 'info');
      const r = await fetch(repoFileUrl(entry.dir, file), {cache:'no-store'});
      if (!r.ok) throw new Error('fetch ' + file + ' -> HTTP ' + r.status);
      map[key] = await r.blob();
    }
    if (entry.persona) await grab(entry.persona, 'persona.md');
    if (entry.settings) { try { await grab(entry.settings, 'settings.json'); } catch(e){} }
    const emotions = entry.emotions || {};
    for (const emo of Object.keys(emotions)){
      const fname = emotions[emo];
      const ext = (fname.split('.').pop() || 'jpg').toLowerCase();
      try { await grab(fname, emo + '.' + ext); } catch(e){ /* skip a missing image; applyEmoji reports the gap */ }
    }
    return await applyProfile(memFiles(map), status);
  }

  Object.assign(VA, {
    ALL_EMOTIONS, FRAME_PX, FRAME_BYTES, MAX_FRAMES, PERSONA_MAX, SRC_EXTS, REPO, BRANCH, REPO_BASE,
    rgba_to_rgb565_le, rgb565_le_to_rgba, drawCover, decodeFile, uploadFrames,
    addEmojiToZip, zipHasEmoji, applyEmoji, applyEmojiFromZip,
    buildProfileZip, applyProfile, applyProfileFromZip, memFiles,
    fetchZipFromUrl, downloadBlob, fetchRepoIndex, repoFileUrl, importRepoPersona
  });
})();
</script>)JS"

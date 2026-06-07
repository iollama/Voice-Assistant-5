// Shared "neo-brutalist sticker" design tokens + base CSS for the captive-portal
// pages. THEME_CSS is a raw-string-literal macro that root_page.h and emojis_page.h
// concatenate inline (adjacent string literals), so each page stays a single
// PROGMEM blob served by send_P -- no heap, no second HTTP round-trip, and the page
// still renders standalone for the captive portal. Palette is defined once in
// :root; page-specific component styles live in each page's own <style> block.
//
// Kept in a .h (not .ino) for the same reason as the page files: the Arduino
// auto-prototyper scans .ino files and chokes on embedded markup/JS.

#pragma once

#define THEME_CSS R"CSS(<style>
:root{--ink:#111;--blue:#1742d1;--red:#ff2d2d;--green:#42c98e;--yellow:#ffe34d;--paper:#fff200;--mute:#666}
*{box-sizing:border-box}
body{font-family:"Comic Sans MS","Arial Black",system-ui,sans-serif;color:var(--ink);margin:0;padding:18px;
     background:var(--paper);background-image:radial-gradient(#0000001a 18%,transparent 19%);background-size:15px 15px}
a{color:var(--blue)}
.footer{text-align:center;margin:28px 0 6px;font-size:13px;font-weight:700;color:var(--ink)}
.footer a{font-weight:900}
</style>)CSS"
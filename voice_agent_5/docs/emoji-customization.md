# Customizing Emojis

The voice agent ships with seven hand-drawn emoji animations: **neutral, happy, excited, empathetic, confused, concerned, thinking**. You can replace any or all of them with your own artwork directly from the device's web interface — no Python, no re-flash, no external service.

## How to replace an emoji

1. Open the device's settings page in a browser (in AP mode, connect to the `VOICE-AGENT-xxxx` WiFi and visit `http://192.168.4.1/`; in normal mode, visit the IP shown on the device's info screen).
2. Click **Open Emoji Settings**, or go straight to `http://<device>/emojis`.
3. Click **Replace** on the emotion you want to change, pick a `.gif`, `.png`, `.jpg`, or `.mp4` file.
4. Your browser converts it to the device's native format and uploads. The new emoji is live immediately.

There is **no risk of bricking the device** through customization. The shipped defaults are read-only and live alongside your overrides. Reset any emotion at any time to fall back to its default.

> **Heads up:** the `/emojis` page itself needs your browser to have internet access — it loads the GIF decoder and zip libraries from a public CDN (`cdn.jsdelivr.net`, pinned with subresource integrity). Once the page is loaded, conversion and upload happen locally; the device never talks to the internet for this. If your phone or laptop is fully offline, the page won't load — connect to a normal network first, or hand-author a pack of raw `.bin` frames (see [Pack format](#pack-format-raw-bin-export) below) and import that instead.

## What to upload

| Source | Result on device |
|---|---|
| GIF (animated) | Up to 8 frames sampled evenly; animates at the same speed as the shipped emojis (~8 fps) |
| GIF (single frame) | 1 frame, displayed static |
| PNG | 1 frame, displayed static |
| JPG | 1 frame, displayed static |
| MP4 (H.264) | 8 stills sampled evenly across the clip; animates at ~8 fps |

Decoding happens in your browser, so MP4 support depends on the browser's built-in player: **H.264 (AVC) in an `.mp4` container works everywhere**. Other codecs/containers (HEVC/H.265 — common in iPhone recordings — VP9, AV1, `.mov`, `.webm`) may fail to decode depending on your browser; if a video won't convert, re-export it as H.264/MP4. Other formats (WebP, APNG, SVG, …) are not supported.

## Tips for good-looking emojis

- **Square aspect ratio.** The display is 150×150 px area on a 240×240 round panel, so non-square images get cover-fit (centered, edges cropped). Square sources give the most predictable result.
- **Center your subject.** The corners of the round display are clipped — anything important needs to be near the middle.
- **Bold colors and shapes work best.** The panel is small (~1.3" / 33 mm) and uses 16-bit color (RGB565). Thin gray lines and subtle gradients can disappear; saturated primary colors pop.
- **Short loops animate smoother.** A GIF with 4 frames at 24 fps gets sampled to 4 frames at 8 fps — your animation will look exactly the same. A 60-frame GIF gets sampled to 8 frames, which can look choppy if the motion was meant to be smooth. For best animation quality, author with 6–8 evenly-paced key frames.
- **File size doesn't matter much.** The browser does the heavy lifting; the device only ever receives a 45 KB per frame (max 360 KB total per emotion).

## Resetting

- **Reset one** (button on each emotion's tile): deletes that emotion's customization, falls back to the shipped default.
- **Reset all to defaults**: clears every customization in one click.

Defaults are stored separately and are never written to from the portal. There is no way to permanently lose them from the captive portal.

## Sharing a pack

The **Export pack** button downloads a `.zip` of all your customizations — useful as a backup or to share with another device's owner. The **Import pack** button accepts that same `.zip` to restore them.

> This page deals with **emoji only**. Export pack includes just the moods you have customized, and Import pack applies only the emoji in a zip — if the zip also contains a `persona.md` or `settings.json` (i.e. it's a full device *profile*), those are **ignored** here. To back up or apply your persona, voice and volume together with the emoji, use **Import / Export Profile** on the main settings page instead — see [profile-import-export.md](profile-import-export.md). Note the two exports differ: `emoji-pack.zip` holds only your customized moods, whereas a profile's `persona.zip` holds all seven moods (custom or shipped default).

### Hand-authoring a pack zip

Import also accepts a zip of source files. Layout:

```
my-pack.zip
├── neutral.gif       (or .png / .jpg)
├── happy.gif
├── excited.png
└── thinking.jpg      (any subset of the 7; missing ones stay default)
```

Filename (minus extension) must be one of the seven emotion names. The browser converts each before uploading, just like a single Replace.

### Pack format (raw `.bin` export)

For reference, the exported zip from a device contains:

```
emoji-pack.zip
├── manifest.json     { "happy": {"count": 3}, "excited": {"count": 8}, ... }
├── happy_0.bin       (45000 bytes, RGB565 little-endian, 150x150)
├── happy_1.bin
├── happy_2.bin
├── excited_0.bin
├── ...
```

Frame files are raw RGB565 little-endian byte arrays of exactly 45000 bytes (150 × 150 × 2). Each emotion's frame count is taken from `manifest.json`.

## Restrictions

- The seven emotion names are fixed. You can change the artwork, but you cannot add a "surprised" emotion or rename "thinking" to "pondering" — the assistant's tool schema knows the seven names and won't call into any others.
- One upload at a time. The portal blocks a second upload until the first finishes.
- Uploads are rejected during an active voice turn (state RECORDING, THINKING, or SPEAKING). Wait for the device to return to green-ready, then try again.

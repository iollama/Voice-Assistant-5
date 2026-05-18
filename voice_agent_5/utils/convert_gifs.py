"""
Convert emoji GIF animations to RGB565 raw binary frames for ESP32 GC9A01 display.

Per docs/architecture.md (Emoji Display System):
- Extract 8 evenly-spaced frames from each GIF
- Resize to 150x150 (LANCZOS)
- Flatten alpha onto black background
- Convert to RGB565 little-endian (ESP32 native byte order)
- Write to data/default/<emotion>_<frame>.bin
"""

import struct
from pathlib import Path
from PIL import Image

PROJECT_DIR = Path(__file__).parent.parent
FEELINGS_DIR = PROJECT_DIR / "feelings"
DATA_DIR = PROJECT_DIR / "data" / "default"
FRAME_SIZE = 150
NUM_FRAMES = 8

EMOTIONS = ["neutral", "happy", "excited", "empathetic", "confused", "concerned", "thinking"]


def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert_gif(emotion: str):
    gif_path = FEELINGS_DIR / f"{emotion}.gif"
    img = Image.open(gif_path)

    # Count total frames
    total_frames = 0
    try:
        while True:
            total_frames += 1
            img.seek(img.tell() + 1)
    except EOFError:
        pass

    # Pick 8 evenly-spaced frame indices
    if total_frames <= NUM_FRAMES:
        indices = list(range(total_frames))
        # Pad by repeating last frame if fewer than 8
        while len(indices) < NUM_FRAMES:
            indices.append(total_frames - 1)
    else:
        indices = [int(i * total_frames / NUM_FRAMES) for i in range(NUM_FRAMES)]

    print(f"  {emotion}: {total_frames} total frames, extracting indices {indices}")

    for out_idx, frame_idx in enumerate(indices):
        img.seek(frame_idx)
        frame = img.convert("RGBA")

        # Flatten alpha onto black background
        background = Image.new("RGBA", frame.size, (0, 0, 0, 255))
        composite = Image.alpha_composite(background, frame)
        composite = composite.convert("RGB")

        # Resize to 150x150
        composite = composite.resize((FRAME_SIZE, FRAME_SIZE), Image.LANCZOS)

        # Convert to RGB565 little-endian (ESP32 native byte order)
        raw = bytearray()
        pixels = composite.load()
        for y in range(FRAME_SIZE):
            for x in range(FRAME_SIZE):
                r, g, b = pixels[x, y]
                rgb565 = rgb888_to_rgb565(r, g, b)
                raw.extend(struct.pack("<H", rgb565))

        out_path = DATA_DIR / f"{emotion}_{out_idx}.bin"
        out_path.write_bytes(raw)
        print(f"    -> {out_path.name} ({len(raw)} bytes)")


def main():
    DATA_DIR.mkdir(exist_ok=True)
    print(f"Converting GIFs from {FEELINGS_DIR}")
    print(f"Output to {DATA_DIR}\n")

    for emotion in EMOTIONS:
        convert_gif(emotion)

    total_files = list(DATA_DIR.glob("*.bin"))
    total_bytes = sum(f.stat().st_size for f in total_files)
    print(f"\nDone: {len(total_files)} files, {total_bytes:,} bytes total ({total_bytes/1024/1024:.2f} MB)")


if __name__ == "__main__":
    main()

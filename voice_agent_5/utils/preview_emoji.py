"""
Preview RGB565 binary frames as animated GIFs in the system image viewer.

Usage:
    python preview_emoji.py              # preview all emotions
    python preview_emoji.py happy        # preview a single emotion
    python preview_emoji.py --save       # save preview GIFs to utils/preview/
"""

import struct
import sys
from pathlib import Path
from PIL import Image

PROJECT_DIR = Path(__file__).parent.parent
DATA_DIR = PROJECT_DIR / "data"
PREVIEW_DIR = Path(__file__).parent / "preview"
FRAME_SIZE = 150
NUM_FRAMES = 8

EMOTIONS = ["neutral", "happy", "excited", "empathetic", "confused", "concerned", "thinking"]


def rgb565_to_rgb888(value):
    r = (value >> 11) & 0x1F
    g = (value >> 5) & 0x3F
    b = value & 0x1F
    return (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)


def load_frame(emotion: str, idx: int) -> Image.Image:
    path = DATA_DIR / f"{emotion}_{idx}.bin"
    data = path.read_bytes()
    img = Image.new("RGB", (FRAME_SIZE, FRAME_SIZE))
    pixels = img.load()
    offset = 0
    for y in range(FRAME_SIZE):
        for x in range(FRAME_SIZE):
            value = struct.unpack("<H", data[offset:offset + 2])[0]
            pixels[x, y] = rgb565_to_rgb888(value)
            offset += 2
    return img


def make_gif(emotion: str) -> list[Image.Image]:
    return [load_frame(emotion, i) for i in range(NUM_FRAMES)]


def main():
    save = "--save" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    emotions = args if args else EMOTIONS

    for e in emotions:
        if e not in EMOTIONS:
            print(f"Unknown emotion: {e}")
            continue

        frames = make_gif(e)
        print(f"{e}: loaded {len(frames)} frames")

        if save:
            PREVIEW_DIR.mkdir(exist_ok=True)
            out = PREVIEW_DIR / f"{e}.gif"
            frames[0].save(out, save_all=True, append_images=frames[1:],
                           duration=120, loop=0)
            print(f"  -> saved {out}")
        else:
            frames[0].save(f"{e}.gif", save_all=True, append_images=frames[1:],
                           duration=120, loop=0)
            img = Image.open(f"{e}.gif")
            img.show(title=e)
            Path(f"{e}.gif").unlink()


if __name__ == "__main__":
    main()

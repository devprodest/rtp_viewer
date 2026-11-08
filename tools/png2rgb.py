#!/usr/bin/env python3
"""
Convert an image (PNG, JPEG, etc.) to raw RGB888 binary.

Output goes next to the input file with the .rgb extension.
Raw layout: R,G,B,R,G,B,...  (8 bit per channel, top-down, no header).

Examples:
    python png2rgb.py image.png
    python png2rgb.py image.png -o /tmp/frame.rgb
    python png2rgb.py image.png --bg ff00ff      # composite alpha on magenta
    python png2rgb.py image.png --bgr            # BGR order instead of RGB
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required:  pip install Pillow")


def parse_color(s: str) -> tuple[int, int, int]:
    s = s.lstrip("#")
    if len(s) == 6:
        return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))
    if len(s) == 3:
        return tuple(int(c * 2, 16) for c in s)
    raise argparse.ArgumentTypeError(
        f"invalid color {s!r}: expected RRGGBB or RGB hex"
    )


def main() -> None:
    p = argparse.ArgumentParser(
        description="Convert image -> raw RGB888 binary (.rgb next to input)."
    )
    p.add_argument("input", type=Path, help="path to input image")
    p.add_argument(
        "-o", "--output", type=Path,
        help="output path (default: same dir as input, .rgb extension)",
    )
    p.add_argument(
        "--bg", type=parse_color, default=(0, 0, 0), metavar="HEX",
        help="background color for compositing alpha channel (default: 000000)",
    )
    p.add_argument(
        "--bgr", action="store_true",
        help="write BGR byte order instead of RGB",
    )
    p.add_argument(
        "-q", "--quiet", action="store_true", help="suppress output message",
    )
    args = p.parse_args()

    src: Path = args.input
    if not src.is_file():
        sys.exit(f"error: not a file: {src}")

    dst: Path = args.output or src.with_suffix(".rgb")

    img = Image.open(src)

    # Composite alpha on solid background (lossless, predictable)
    if img.mode in ("RGBA", "LA", "P"):
        img = img.convert("RGBA")
        bg = Image.new("RGBA", img.size, (*args.bg, 255))
        img = Image.alpha_composite(bg, img).convert("RGB")
    else:
        img = img.convert("RGB")

    data = img.tobytes()  # exactly R,G,B,R,G,B,... — that IS rgb888

    if args.bgr:
        import numpy as np
        arr = np.frombuffer(data, dtype=np.uint8).reshape(img.height, img.width, 3)
        data = arr[..., ::-1].tobytes()

    dst.write_bytes(data)

    expected = img.width * img.height * 3
    if len(data) != expected:                     # sanity guard
        sys.exit(f"error: size mismatch {len(data)} != {expected}")

    if not args.quiet:
        order = "BGR" if args.bgr else "RGB"
        print(f"{src} -> {dst}  "
              f"({img.width}x{img.height}, {len(data)} bytes, {order}888)")


if __name__ == "__main__":
    main()
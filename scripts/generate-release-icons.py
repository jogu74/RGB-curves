#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image


ICONSET_SPECS = [
    ("icon_16x16.png", 16),
    ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32),
    ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128),
    ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256),
    ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512),
    ("icon_512x512@2x.png", 1024),
]

ICO_SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def square_canvas(image: Image.Image, size: int) -> Image.Image:
    image = image.convert("RGBA")
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    scale = min(size / image.width, size / image.height)
    scaled_size = (
        max(1, round(image.width * scale)),
        max(1, round(image.height * scale)),
    )
    scaled = image.resize(scaled_size, Image.Resampling.LANCZOS)

    offset = ((size - scaled.width) // 2, (size - scaled.height) // 2)
    canvas.alpha_composite(scaled, dest=offset)
    return canvas


def build_icns(source: Path, output: Path) -> None:
    iconutil = shutil.which("iconutil")
    if not iconutil:
        raise RuntimeError("iconutil is required to generate .icns files on macOS")

    with tempfile.TemporaryDirectory(prefix="colorforge-iconset-") as temp_dir:
        iconset_dir = Path(temp_dir) / "ColorForge.iconset"
        iconset_dir.mkdir(parents=True, exist_ok=True)

        for filename, size in ICONSET_SPECS:
            square_canvas(Image.open(source), size).save(iconset_dir / filename)

        output.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [iconutil, "-c", "icns", str(iconset_dir), "-o", str(output)],
            check=True,
        )


def build_ico(source: Path, output: Path) -> None:
    image = Image.open(source)
    square = square_canvas(image, 1024)
    output.parent.mkdir(parents=True, exist_ok=True)
    square.save(output, format="ICO", sizes=ICO_SIZES)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate ColorForge installer icons")
    parser.add_argument("source", type=Path, help="Source PNG artwork")
    parser.add_argument("--icns", type=Path, help="Output .icns path")
    parser.add_argument("--ico", type=Path, required=True, help="Output .ico path")
    args = parser.parse_args()

    if not args.source.is_file():
        raise FileNotFoundError(f"Missing source image: {args.source}")

    build_ico(args.source, args.ico)
    print(f"ICO={args.ico}")

    if args.icns is not None:
        try:
            build_icns(args.source, args.icns)
        except Exception as exc:  # pragma: no cover - best-effort packaging helper
            if args.icns.exists():
                args.icns.unlink()
            print(f"ICNS_WARNING={exc}")
        else:
            print(f"ICNS={args.icns}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

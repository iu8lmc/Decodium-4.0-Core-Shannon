#!/usr/bin/env python3

import argparse
import struct
import sys
from pathlib import Path


ICON_CHUNKS = (
    ("icp4", "icon_16x16.png"),
    ("ic11", "icon_16x16@2x.png"),
    ("icp5", "icon_32x32.png"),
    ("ic12", "icon_32x32@2x.png"),
    ("ic07", "icon_128x128.png"),
    ("ic13", "icon_128x128@2x.png"),
    ("ic08", "icon_256x256.png"),
    ("ic14", "icon_256x256@2x.png"),
    ("ic09", "icon_512x512.png"),
    ("ic10", "icon_512x512@2x.png"),
)


def build_icns(iconset_dir: Path, output_file: Path) -> None:
    body = bytearray()

    for chunk_type, filename in ICON_CHUNKS:
        png_path = iconset_dir / filename
        if not png_path.is_file():
            raise FileNotFoundError(f"missing iconset asset: {png_path}")

        data = png_path.read_bytes()
        if not data.startswith(b"\x89PNG\r\n\x1a\n"):
            raise ValueError(f"icon asset is not a PNG file: {png_path}")

        body += chunk_type.encode("ascii")
        body += struct.pack(">I", len(data) + 8)
        body += data

    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_bytes(b"icns" + struct.pack(">I", len(body) + 8) + body)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Build a macOS .icns file from a .iconset directory")
    parser.add_argument("--iconset", required=True, help="Path to the source .iconset directory")
    parser.add_argument("--output", required=True, help="Path to the output .icns file")
    args = parser.parse_args(argv)

    iconset_dir = Path(args.iconset)
    output_file = Path(args.output)

    if iconset_dir.suffix != ".iconset":
        raise ValueError(f"expected a .iconset directory, got: {iconset_dir}")
    if not iconset_dir.is_dir():
        raise FileNotFoundError(f"iconset directory not found: {iconset_dir}")

    build_icns(iconset_dir, output_file)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as exc:  # pragma: no cover - build-time error reporting
        print(f"make_icns.py: {exc}", file=sys.stderr)
        raise SystemExit(1)

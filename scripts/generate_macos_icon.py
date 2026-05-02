#!/usr/bin/env python3
"""Generate AutoWhisper macOS .iconset PNGs using only the Python stdlib."""
from __future__ import annotations

import argparse
import math
import struct
import zlib
from pathlib import Path

SIZES = {
    "icon_16x16.png": 16,
    "icon_16x16@2x.png": 32,
    "icon_32x32.png": 32,
    "icon_32x32@2x.png": 64,
    "icon_128x128.png": 128,
    "icon_128x128@2x.png": 256,
    "icon_256x256.png": 256,
    "icon_256x256@2x.png": 512,
    "icon_512x512.png": 512,
    "icon_512x512@2x.png": 1024,
}


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def write_png(path: Path, size: int) -> None:
    rows = []
    cx = cy = (size - 1) / 2.0
    radius = size * 0.46
    mic_w = size * 0.20
    mic_h = size * 0.46
    stem_w = max(1.0, size * 0.055)
    paper = (246, 242, 229, 255)
    ink = (28, 31, 36, 255)
    signal = (28, 111, 226, 255)
    shadow = (15, 23, 42, 68)

    for y in range(size):
        row = bytearray([0])
        for x in range(size):
            dx = x - cx
            dy = y - cy
            dist = math.sqrt(dx * dx + dy * dy)
            if dist > radius:
                rgba = (0, 0, 0, 0)
            else:
                # Soft circular paper background.
                rgba = paper
                # Blue signal dot.
                dot_r = size * 0.075
                dot_cx = cx + size * 0.24
                dot_cy = cy - size * 0.24
                if math.sqrt((x - dot_cx) ** 2 + (y - dot_cy) ** 2) <= dot_r:
                    rgba = signal
                # Mic body rounded capsule.
                body_cx = cx
                body_top = cy - mic_h * 0.55
                body_bottom = cy + mic_h * 0.30
                half_w = mic_w / 2
                if abs(x - body_cx) <= half_w and body_top <= y <= body_bottom:
                    rgba = ink
                if math.sqrt((x - body_cx) ** 2 + (y - body_top) ** 2) <= half_w and y < body_top + half_w:
                    rgba = ink
                if math.sqrt((x - body_cx) ** 2 + (y - body_bottom) ** 2) <= half_w and y > body_bottom - half_w:
                    rgba = ink
                # Mic stand.
                if abs(x - cx) <= stem_w and cy + mic_h * 0.28 <= y <= cy + mic_h * 0.56:
                    rgba = ink
                if cy + mic_h * 0.55 <= y <= cy + mic_h * 0.62 and abs(x - cx) <= mic_w * 0.75:
                    rgba = ink
                # Slight lower-left shadow in outer ring.
                if radius * 0.82 < dist <= radius and dx < 0 and dy > 0 and rgba == paper:
                    rgba = shadow
            row.extend(rgba)
        rows.append(bytes(row))
    raw = b"".join(rows)
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9))
    data += chunk(b"IEND", b"")
    path.write_bytes(data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, size in SIZES.items():
        write_png(args.output / name, size)


if __name__ == "__main__":
    main()

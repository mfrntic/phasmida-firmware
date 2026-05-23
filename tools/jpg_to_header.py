"""
Convert a JPEG file to a C/C++ PROGMEM header for use with M5GFX / LovyanGFX drawJpg().

Resizes the image to TARGET_SIZE x TARGET_SIZE pixels before embedding so that
drawJpg() draws it at the correct pixel dimensions without any runtime scaling.

Usage:
    python tools/jpg_to_header.py

Reads:  phasmida_sticker_small.jpg  (project root)
Writes: include/ui/splash_logo.h

Constants written to header:
    kSplashLogoJpg     — PROGMEM byte array
    kSplashLogoJpgLen  — byte count
    kSplashLogoWidth   — pixel width  (TARGET_SIZE)
    kSplashLogoHeight  — pixel height (TARGET_SIZE)
"""

import io
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Run: pip install Pillow", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)

INPUT_FILE  = os.path.join(PROJECT_ROOT, "phasmida_sticker_small.jpg")
OUTPUT_FILE = os.path.join(PROJECT_ROOT, "include", "ui", "splash_logo.h")

# Image is already 320x240 — no resize needed
JPEG_QUALITY = 85

GUARD  = "SPLASH_LOGO_H"
ARRAY  = "kSplashLogoJpg"
LENGTH = "kSplashLogoJpgLen"
WIDTH  = "kSplashLogoWidth"
HEIGHT = "kSplashLogoHeight"

def main():
    if not os.path.isfile(INPUT_FILE):
        print(f"ERROR: input file not found: {INPUT_FILE}", file=sys.stderr)
        sys.exit(1)

    img = Image.open(INPUT_FILE).convert("RGB")
    orig_w, orig_h = img.size

    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=JPEG_QUALITY, optimize=True)
    data = buf.getvalue()

    print(f"Input:  {INPUT_FILE}  ({orig_w}x{orig_h} px, {os.path.getsize(INPUT_FILE)} bytes)")
    print(f"Embedded: {orig_w}x{orig_h} px, quality={JPEG_QUALITY} → {len(data)} bytes")

    COLS = 16
    rows = []
    for i in range(0, len(data), COLS):
        chunk = data[i:i + COLS]
        rows.append("  " + ", ".join(f"0x{b:02X}" for b in chunk))

    lines = [
        f"#ifndef {GUARD}",
        f"#define {GUARD}",
        "",
        "#include <Arduino.h>  // PROGMEM",
        "",
        f"// Auto-generated from phasmida_sticker_small.jpg — do not edit.",
        f"// Regenerate with: python tools/jpg_to_header.py",
        f"// Embedded size: {orig_w}x{orig_h} px, {len(data)} bytes",
        "",
        f"static const uint16_t {WIDTH}  = {orig_w};",
        f"static const uint16_t {HEIGHT} = {orig_h};",
        "",
        f"static const uint8_t {ARRAY}[] PROGMEM = {{",
    ]
    lines.extend(",\n".join(rows).split("\n"))
    # Fix trailing comma on last data row
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]
    lines += [
        "};",
        "",
        f"static const size_t {LENGTH} = sizeof({ARRAY});",
        "",
        f"#endif  // {GUARD}",
        "",
    ]

    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    with open(OUTPUT_FILE, "w", newline="\n") as f:
        f.write("\n".join(lines))

    print(f"Output: {OUTPUT_FILE}  ({len(lines)} lines, {len(data)} bytes embedded)")

if __name__ == "__main__":
    main()

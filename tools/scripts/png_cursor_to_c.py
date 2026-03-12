#!/usr/bin/env python3

import argparse
import os
import struct
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

# Shared standard-cursor manifest. Backends can consume the generated table as needed.
CURSOR_MANIFEST = [
    ("default", "MARU_CURSOR_SHAPE_DEFAULT", 1, 14),
    ("help", "MARU_CURSOR_SHAPE_HELP", 8, 8),
    ("hand", "MARU_CURSOR_SHAPE_HAND", 8, 12),
    ("wait", "MARU_CURSOR_SHAPE_WAIT", 8, 8),
    ("crosshair", "MARU_CURSOR_SHAPE_CROSSHAIR", 8, 8),
    ("text", "MARU_CURSOR_SHAPE_TEXT", 8, 8),
    ("move", "MARU_CURSOR_SHAPE_MOVE", 8, 8),
    ("not_allowed", "MARU_CURSOR_SHAPE_NOT_ALLOWED", 8, 8),
    ("ew_resize", "MARU_CURSOR_SHAPE_EW_RESIZE", 8, 8),
    ("ns_resize", "MARU_CURSOR_SHAPE_NS_RESIZE", 8, 8),
    ("nesw_resize", "MARU_CURSOR_SHAPE_NESW_RESIZE", 8, 8),
    ("nwse_resize", "MARU_CURSOR_SHAPE_NWSE_RESIZE", 8, 8),
]


def _paeth_predictor(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter_rows(raw, width, height, bytes_per_pixel):
    stride = width * bytes_per_pixel
    out = bytearray(height * stride)
    pos = 0
    dst = 0
    for _ in range(height):
        filter_type = raw[pos]
        pos += 1
        row = bytearray(raw[pos : pos + stride])
        pos += stride

        if filter_type == 1:
            for i in range(bytes_per_pixel, stride):
                row[i] = (row[i] + row[i - bytes_per_pixel]) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                up = out[dst - stride + i] if dst >= stride else 0
                row[i] = (row[i] + up) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = row[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
                up = out[dst - stride + i] if dst >= stride else 0
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                left = row[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
                up = out[dst - stride + i] if dst >= stride else 0
                up_left = out[dst - stride + i - bytes_per_pixel] if (dst >= stride and i >= bytes_per_pixel) else 0
                row[i] = (row[i] + _paeth_predictor(left, up, up_left)) & 0xFF
        elif filter_type != 0:
            raise ValueError(f"Unsupported PNG filter type: {filter_type}")

        out[dst : dst + stride] = row
        dst += stride
    return bytes(out)


def decode_png_rgba(path):
    with open(path, "rb") as f:
        data = f.read()

    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"{path}: invalid PNG signature")

    offset = len(PNG_SIGNATURE)
    width = None
    height = None
    bit_depth = None
    color_type = None
    compression = None
    filter_method = None
    interlace = None
    idat_parts = []

    while offset + 12 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        chunk_data = data[offset + 8 : offset + 8 + length]
        offset += 12 + length

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
                ">IIBBBBB", chunk_data
            )
        elif chunk_type == b"IDAT":
            idat_parts.append(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise ValueError(f"{path}: missing IHDR")
    if bit_depth != 8:
        raise ValueError(f"{path}: only 8-bit PNGs are supported")
    if interlace != 0:
        raise ValueError(f"{path}: interlaced PNGs are not supported")
    if compression != 0 or filter_method != 0:
        raise ValueError(f"{path}: unsupported PNG compression/filter method")

    if color_type == 6:
        source_bpp = 4
    elif color_type == 2:
        source_bpp = 3
    elif color_type == 0:
        source_bpp = 1
    elif color_type == 4:
        source_bpp = 2
    else:
        raise ValueError(f"{path}: unsupported PNG color type {color_type}")

    raw = zlib.decompress(b"".join(idat_parts))
    expected_size = height * (1 + width * source_bpp)
    if len(raw) != expected_size:
        raise ValueError(f"{path}: unexpected decompressed size")
    scanlines = _unfilter_rows(raw, width, height, source_bpp)

    rgba = bytearray(width * height * 4)
    src = 0
    dst = 0
    pixel_count = width * height
    for _ in range(pixel_count):
        if color_type == 6:
            r, g, b, a = scanlines[src], scanlines[src + 1], scanlines[src + 2], scanlines[src + 3]
            src += 4
        elif color_type == 2:
            r, g, b, a = scanlines[src], scanlines[src + 1], scanlines[src + 2], 255
            src += 3
        elif color_type == 0:
            r = g = b = scanlines[src]
            a = 255
            src += 1
        else:  # color_type == 4
            r = g = b = scanlines[src]
            a = scanlines[src + 1]
            src += 2

        rgba[dst : dst + 4] = bytes((r, g, b, a))
        dst += 4

    return width, height, bytes(rgba)


def rgba_to_argb_words(rgba_bytes):
    words = []
    for i in range(0, len(rgba_bytes), 4):
        r = rgba_bytes[i]
        g = rgba_bytes[i + 1]
        b = rgba_bytes[i + 2]
        a = rgba_bytes[i + 3]
        words.append((a << 24) | (r << 16) | (g << 8) | b)
    return words


def generate(output_c, output_h, input_dir):
    datasets = []
    for name, shape_enum, hot_x, hot_y in CURSOR_MANIFEST:
        path = os.path.join(input_dir, f"{name}.png")
        if not os.path.exists(path):
            raise FileNotFoundError(f"Missing cursor asset: {path}")
        width, height, rgba = decode_png_rgba(path)
        words = rgba_to_argb_words(rgba)
        datasets.append((name, shape_enum, hot_x, hot_y, width, height, words))

    guard = os.path.basename(output_h).replace(".", "_").upper() + "_INCLUDED"
    with open(output_h, "w", encoding="utf-8") as h:
        h.write("// Generated by tools/scripts/png_cursor_to_c.py. Do not edit.\n")
        h.write(f"#ifndef {guard}\n")
        h.write(f"#define {guard}\n\n")
        h.write("#include <stdint.h>\n\n")
        h.write('#include "maru/maru.h"\n\n')
        h.write("typedef struct MARU_CursorBitmapAsset {\n")
        h.write("  uint32_t width;\n")
        h.write("  uint32_t height;\n")
        h.write("  uint32_t stride_bytes;\n")
        h.write("  int32_t hot_x;\n")
        h.write("  int32_t hot_y;\n")
        h.write("  const uint32_t *pixels_argb;\n")
        h.write("} MARU_CursorBitmapAsset;\n\n")
        h.write("extern const MARU_CursorBitmapAsset g_maru_cursor_bitmap_assets[MARU_CURSOR_SHAPE_NWSE_RESIZE + 1u];\n\n")
        h.write(f"#endif  // {guard}\n")

    with open(output_c, "w", encoding="utf-8") as c:
        c.write("// Generated by tools/scripts/png_cursor_to_c.py. Do not edit.\n")
        c.write(f'#include "{os.path.basename(output_h)}"\n\n')

        for name, _, _, _, _, _, words in datasets:
            var = f"g_maru_cursor_pixels_{name}"
            c.write(f"static const uint32_t {var}[] = {{\n")
            for i, word in enumerate(words):
                c.write(f"  0x{word:08X}u,")
                if (i + 1) % 8 == 0:
                    c.write("\n")
                else:
                    c.write(" ")
            if len(words) % 8 != 0:
                c.write("\n")
            c.write("};\n\n")

        c.write("const MARU_CursorBitmapAsset g_maru_cursor_bitmap_assets[MARU_CURSOR_SHAPE_NWSE_RESIZE + 1u] = {\n")
        for name, shape_enum, hot_x, hot_y, width, height, _ in datasets:
            var = f"g_maru_cursor_pixels_{name}"
            c.write(f"  [{shape_enum}] = {{\n")
            c.write(f"    .width = {width}u,\n")
            c.write(f"    .height = {height}u,\n")
            c.write(f"    .stride_bytes = {width * 4}u,\n")
            c.write(f"    .hot_x = {hot_x},\n")
            c.write(f"    .hot_y = {hot_y},\n")
            c.write(f"    .pixels_argb = {var},\n")
            c.write("  },\n")
        c.write("};\n")


def main():
    parser = argparse.ArgumentParser(
        description="Convert cursor PNG assets to shared C bitmap tables."
    )
    parser.add_argument("--input-dir", required=True, help="Directory containing cursor PNG files.")
    parser.add_argument("--output-c", required=True, help="Generated C source path.")
    parser.add_argument("--output-h", required=True, help="Generated C header path.")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output_c), exist_ok=True)
    os.makedirs(os.path.dirname(args.output_h), exist_ok=True)
    generate(args.output_c, args.output_h, args.input_dir)


if __name__ == "__main__":
    main()

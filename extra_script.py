# See https://docs.platformio.org/en/latest/manifests/library-json/fields/build/extrascript.html
Import("env")
from os.path import join, realpath
from pathlib import Path
# Base srcFilter. Cannot be set in library.json.
src_filter = [
    "+<resources>",
    "+<locale>",
    "+<source>"
]

for item in env.get("CPPDEFINES", []):
    # Add generated view directory to include path dependending on VIEW_* macro
    if isinstance(item,str) and item.startswith("VIEW_"):
        view = f"ui_{item[5:]}".lower()  # Ex value: "ui_320x240"
        env.Append(CPPPATH=[realpath(join("generated", view))])
        src_filter.append(f"+<generated/{view}>")
    # Add portduino directory to include path dependending on ARCH_PORTDUINO macro
    elif item == "ARCH_PORTDUINO":
        env.Append(CPPPATH=[realpath("portduino")])
        src_filter.append("+<portduino>")

# Only `Replace` is supported for SRC_FILTER, not `Append` or `Prepend`
env.Replace(SRC_FILTER=src_filter)

font_source = env.GetProjectOption("custom_meshtastic_japanese_font", None)
if font_source:
    project_dir = Path(env.subst("$PROJECT_DIR"))
    font_path = (project_dir / font_source).resolve()
    if not font_path.is_file():
        raise RuntimeError(f"Japanese font file not found: {font_path}")

    glyphs = {}
    ascent, descent = 10, 2
    glyph = None
    bitmap_rows = None

    def decode_bdf_encoding(encoding):
        if encoding < 0:
            return None
        row, cell = encoding >> 8, encoding & 0xFF
        if 0x21 <= row <= 0x7E and 0x21 <= cell <= 0x7E:
            try:
                decoded = bytes((row + 0x80, cell + 0x80)).decode("euc_jp")
                return ord(decoded) if len(decoded) == 1 else None
            except UnicodeDecodeError:
                return None
        return encoding

    for line in font_path.read_text(encoding="ascii").splitlines():
        if line.startswith("FONT_ASCENT "):
            ascent = int(line.split()[1])
        elif line.startswith("FONT_DESCENT "):
            descent = int(line.split()[1])
        elif line.startswith("STARTCHAR "):
            glyph = {}
            bitmap_rows = None
        elif glyph is not None and line.startswith("ENCODING "):
            glyph["codepoint"] = decode_bdf_encoding(int(line.split()[1]))
        elif glyph is not None and line.startswith("DWIDTH "):
            glyph["advance"] = int(line.split()[1])
        elif glyph is not None and line.startswith("BBX "):
            glyph["width"], glyph["height"], glyph["offset_x"], glyph["offset_y"] = map(int, line.split()[1:])
        elif glyph is not None and line == "BITMAP":
            bitmap_rows = []
        elif glyph is not None and line == "ENDCHAR":
            codepoint = glyph.get("codepoint")
            if codepoint is not None and codepoint > 0x7F:
                width, height = glyph["width"], glyph["height"]
                stride = (width + 7) // 8
                raw_bitmap = b"".join(bytes.fromhex(row) for row in bitmap_rows or [])
                if width <= 0 or height <= 0 or len(raw_bitmap) != stride * height:
                    raise RuntimeError(f"Invalid Japanese glyph bitmap for U+{codepoint:04X}")
                glyphs[codepoint] = dict(glyph, bitmap=raw_bitmap, bitmap_stride=stride)
            glyph = None
            bitmap_rows = None
        elif glyph is not None and bitmap_rows is not None:
            bitmap_rows.append(line)

    selected = [glyphs[codepoint] for codepoint in sorted(glyphs)]
    if not selected:
        raise RuntimeError(f"No Japanese glyphs found in {font_path}")

    bitmap = bytearray()
    for glyph in selected:
        glyph["bitmap_offset"] = len(bitmap)
        bitmap.extend(glyph["bitmap"])

    header_lines = [
        "#pragma once",
        "#include <lvgl.h>",
        "#include <cstddef>",
        "#include <cstdint>",
        "extern const lv_font_t ui_font_montserrat_14;",
        "namespace {",
        "struct DeviceUiJapaneseGlyph { uint32_t codepoint, bitmapOffset; uint16_t advance, width, height, bitmapStride; int16_t offsetX, offsetY; };",
        "static const uint8_t deviceUiJapaneseBitmap[] = {",
    ]
    for offset in range(0, len(bitmap), 24):
        header_lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in bitmap[offset : offset + 24]) + ",")
    header_lines.append("};")
    header_lines.append("static const DeviceUiJapaneseGlyph deviceUiJapaneseGlyphs[] = {")
    for glyph in selected:
        header_lines.append(
            f"    {{{glyph['codepoint']}u, {glyph['bitmap_offset']}u, {glyph['advance']}, {glyph['width']}, "
            f"{glyph['height']}, {glyph['bitmap_stride']}, {glyph['offset_x']}, {glyph['offset_y']}}},"
        )
    header_lines.extend([
        "};",
        "static bool deviceUiJapaneseGetGlyphDsc(const lv_font_t *, lv_font_glyph_dsc_t *dsc, uint32_t letter, uint32_t)",
        "{",
        "    size_t low = 0, high = sizeof(deviceUiJapaneseGlyphs) / sizeof(deviceUiJapaneseGlyphs[0]);",
        "    while (low < high) {",
        "        const size_t middle = low + (high - low) / 2;",
        "        if (deviceUiJapaneseGlyphs[middle].codepoint < letter) low = middle + 1; else high = middle;",
        "    }",
        "    if (low == sizeof(deviceUiJapaneseGlyphs) / sizeof(deviceUiJapaneseGlyphs[0]) || deviceUiJapaneseGlyphs[low].codepoint != letter) return false;",
        "    const DeviceUiJapaneseGlyph &glyph = deviceUiJapaneseGlyphs[low];",
        "    dsc->adv_w = glyph.advance; dsc->box_w = glyph.width; dsc->box_h = glyph.height;",
        "    dsc->ofs_x = glyph.offsetX; dsc->ofs_y = glyph.offsetY; dsc->stride = glyph.width;",
        "    dsc->format = LV_FONT_GLYPH_FORMAT_A8; dsc->gid.index = static_cast<uint32_t>(low);",
        "    return true;",
        "}",
        "static const void *deviceUiJapaneseGetGlyphBitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *drawBuf)",
        "{",
        "    const size_t count = sizeof(deviceUiJapaneseGlyphs) / sizeof(deviceUiJapaneseGlyphs[0]);",
        "    if (!drawBuf || !drawBuf->data || dsc->gid.index >= count) return nullptr;",
        "    const DeviceUiJapaneseGlyph &glyph = deviceUiJapaneseGlyphs[dsc->gid.index];",
        "    const uint32_t outStride = drawBuf->header.stride;",
        "    if (outStride < glyph.width) return nullptr;",
        "    const uint8_t *source = deviceUiJapaneseBitmap + glyph.bitmapOffset;",
        "    for (uint16_t y = 0; y < glyph.height; ++y) {",
        "        uint8_t *row = drawBuf->data + y * outStride;",
        "        for (uint16_t x = 0; x < glyph.width; ++x)",
        "            row[x] = (source[y * glyph.bitmapStride + x / 8] & (0x80u >> (x % 8))) ? 255 : 0;",
        "    }",
        "    return drawBuf;",
        "}",
        "}",
        "static const lv_font_t device_ui_japanese_font = [] {",
        "    lv_font_t font{};",
        "    font.fallback = &ui_font_montserrat_14;",
        "    font.get_glyph_dsc = deviceUiJapaneseGetGlyphDsc;",
        "    font.get_glyph_bitmap = deviceUiJapaneseGetGlyphBitmap;",
        "    font.line_height = " + str(max(ascent + descent, 19)) + ";",
        "    font.base_line = " + str(max(descent, 4)) + ";",
        "    font.cap_height = " + str(ascent) + ";",
        "    font.x_height = " + str(ascent) + ";",
        "    font.subpx = LV_FONT_SUBPX_NONE;",
        "    font.kerning = LV_FONT_KERNING_NONE;",
        "    font.static_bitmap = 0;",
        "    return font;",
        "}();",
    ])

    build_dir_value = env.subst("$BUILD_DIR")
    if not build_dir_value:
        raise RuntimeError("PlatformIO build directory is unavailable for Japanese font generation")
    build_dir = Path(build_dir_value)
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "device_ui_japanese_font.h").write_text("\n".join(header_lines) + "\n", encoding="utf-8")
    env.Append(CPPPATH=[str(build_dir)], CPPDEFINES=["MESHTASTIC_DEVICE_UI_JAPANESE_FONT"])

# Dump construction environment (for debug purposes)
# print("meshtastic-device-ui Library ENV:")
# print(env.Dump())

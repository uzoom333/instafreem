"""Build a sideloadable Instagram 452.0.1 IPA with the stable dylib, grayscale icon and splash."""

import io
import os
import plistlib
import struct
import zipfile

from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get(
    "INSTAGRAM_DECRYPTED",
    r"C:\Users\aerom\Downloads\com.burbn.instagram-452.0.0-Decrypted",
)
DYLIB = os.path.join(ROOT, "packages", "InstagramMod.dylib")
OUT = os.path.join(ROOT, "packages", "Instagram.ipa")
LOAD_PATH = b"@executable_path/InstagramMod.dylib"
LC_LOAD_DYLIB = 0xC
FONTS = [
    os.environ.get("INSTAFREEM_FONT", ""),
    r"C:\Windows\Fonts\arial.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
]

METAL = {
    "Metal@2x.png": 120,
    "Metal@3x.png": 180,
    "Metal-iPad@2x.png": 152,
    "Metal-iPadPro@2x.png": 167,
}
PULSE_PREFIX = "IGLaunchScreenPulseGradient"


def patch_binary(data: bytes) -> bytes:
    magic, cputype, cpusub, filetype, ncmds, sizeofcmds, flags, reserved = struct.unpack_from("<8I", data, 0)
    if magic != 0xFEEDFACF or filetype != 2:
        raise SystemExit("Instagram binary is not a thin arm64 executable")
    header_end = 32 + sizeofcmds
    min_section = None
    off = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == LC_LOAD_DYLIB:
            name_off = struct.unpack_from("<I", data, off + 8)[0]
            if data[off + name_off : off + cmdsize].split(b"\x00", 1)[0] == LOAD_PATH:
                # injetar de novo carrega o tweak duas vezes e o app fecha na splash
                raise SystemExit("o binario de origem ja tem o InstagramMod.dylib; use a pasta descriptografada original")
        if cmd == 0x19:
            nsects = struct.unpack_from("<I", data, off + 64)[0]
            sec = off + 72
            for _s in range(nsects):
                section_offset = struct.unpack_from("<I", data, sec + 48)[0]
                if section_offset and (min_section is None or section_offset < min_section):
                    min_section = section_offset
                sec += 80
        off += cmdsize
    path = LOAD_PATH + b"\x00"
    cmdsize = (24 + len(path) + 7) & ~7
    if min_section is None or header_end + cmdsize > min_section:
        raise SystemExit(f"not enough header slack ({header_end} + {cmdsize} vs {min_section})")
    blob = struct.pack("<IIIIII", LC_LOAD_DYLIB, cmdsize, 24, 0, 0x00010000, 0x00010000)
    blob += path
    blob = blob.ljust(cmdsize, b"\x00")
    out = bytearray(data)
    out[header_end : header_end + cmdsize] = blob
    struct.pack_into("<II", out, 16, ncmds + 1, sizeofcmds + cmdsize)
    return bytes(out)


def grayscale(im: Image.Image, size: int) -> Image.Image:
    rgba = im.convert("RGBA")
    r, g, b, a = rgba.split()
    gray = ImageOps.grayscale(Image.merge("RGB", (r, g, b)))
    gray = ImageOps.autocontrast(gray, cutoff=2)
    merged = Image.merge("RGBA", (gray, gray, gray, a))
    if merged.size != (size, size):
        merged = merged.resize((size, size), Image.Resampling.LANCZOS)
    return merged


def splash_text(size: int) -> Image.Image:
    im = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    draw = ImageDraw.Draw(im)
    font = splash_font(max(12, size // 7))
    text = "modded by\nAeromorto"
    box = draw.multiline_textbbox((0, 0), text, font=font, align="center", spacing=2)
    width = box[2] - box[0]
    height = box[3] - box[1]
    xy = ((size - width) / 2 - box[0], (size - height) / 2 - box[1])
    draw.multiline_text(xy, text, font=font, fill=(255, 255, 255, 255), align="center", spacing=2)
    return im


def splash_font(size: int):
    for path in FONTS:
        if path and os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default(size)


def png_bytes(im: Image.Image) -> bytes:
    buf = io.BytesIO()
    im.save(buf, format="PNG")
    return buf.getvalue()


def uleb_at(data: bytearray, index: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        byte = data[index]
        index += 1
        value |= (byte & 0x7F) << shift
        if byte < 128:
            return value, index
        shift += 7


def bind_ordinal(symbol: str) -> int:
    if any(part in symbol for part in ("objc", "sel_", "class_", "method_", "object_get")):
        return 2
    return 1


def patch_bind_blob(data: bytearray, start: int, size: int) -> int:
    index = start
    end = start + size
    symbol = ""
    patched = 0
    while index < end:
        op = data[index]
        if op == 0:
            if index + 1 >= end or data[index + 1] == 0:
                return patched
            index += 1
            continue
        opcode = op & 0xF0
        if opcode == 0x40:
            zero = data.index(b"\x00", index + 1)
            symbol = data[index + 1 : zero].decode()
            index = zero + 1
            continue
        if opcode == 0x30:
            data[index] = 0x10 | bind_ordinal(symbol)
            patched += 1
            index += 1
            continue
        if opcode in (0x10, 0x50, 0x90, 0xB0):
            index += 1
            continue
        if opcode in (0x20, 0x60, 0x70, 0x80, 0xA0):
            _, index = uleb_at(data, index + 1)
            continue
        if opcode == 0xC0:
            _, index = uleb_at(data, index + 1)
            _, index = uleb_at(data, index)
            continue
        raise SystemExit("unknown bind opcode %x" % op)


def first_section_offset(data: bytes) -> int:
    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 32
    earliest = None
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == 0x19:
            nsects = struct.unpack_from("<I", data, off + 64)[0]
            sec = off + 72
            for _s in range(nsects):
                section_offset = struct.unpack_from("<I", data, sec + 48)[0]
                if section_offset and (earliest is None or section_offset < earliest):
                    earliest = section_offset
                sec += 80
        off += cmdsize
    if earliest is None:
        raise SystemExit("dylib has no sections")
    return earliest


def remove_load_commands(data: bytearray, drop: set[int]) -> int:
    ncmds, sizeofcmds = struct.unpack_from("<II", data, 16)
    off = 32
    kept = bytearray()
    removed = 0
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmdsize < 8 or off + cmdsize > 32 + sizeofcmds:
            raise SystemExit("corrupt load command")
        if cmd in drop:
            removed += 1
        else:
            kept += data[off : off + cmdsize]
        off += cmdsize
    if off != 32 + sizeofcmds:
        raise SystemExit("load commands do not match sizeofcmds")
    if removed == 0:
        return 0
    data[32 : 32 + sizeofcmds] = b"\x00" * sizeofcmds
    data[32 : 32 + len(kept)] = kept
    struct.pack_into("<II", data, 16, ncmds - removed, len(kept))
    return removed


def normalize_for_signing(data: bytearray) -> None:
    """Page-align __TEXT so the signer can emit a valid execSegLimit.

    Sideloadly copies __TEXT.vmsize into the code directory. The kernel
    rejects the signature when that value is not a multiple of 4096.
    """
    remove_load_commands(data, {0xE})
    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 32
    text = None
    segments = []
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == 0x19:
            name = bytes(data[off + 8 : off + 24]).split(b"\x00", 1)[0]
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<QQQQ", data, off + 24)
            segments.append((off, name, vmaddr, vmsize, fileoff, filesize))
            if name == b"__TEXT":
                text = segments[-1]
        off += cmdsize
    if text is None:
        raise SystemExit("dylib has no __TEXT")
    index = next(i for i, seg in enumerate(segments) if seg[1] == b"__TEXT")
    if index + 1 >= len(segments):
        raise SystemExit("dylib __TEXT is the last segment")
    nxt = segments[index + 1]
    new_vmsize = nxt[2] - text[2]
    new_filesize = nxt[4] - text[4]
    if new_vmsize % 0x1000 or new_filesize % 0x1000 or new_vmsize < text[3] or new_filesize < text[5]:
        raise SystemExit("cannot page-align __TEXT")
    if any(data[text[4] + text[5] : text[4] + new_filesize]):
        raise SystemExit("__TEXT padding is not zero")
    struct.pack_into("<Q", data, text[0] + 32, new_vmsize)
    struct.pack_into("<Q", data, text[0] + 48, new_filesize)
    cryptoff = 0x4000
    cryptsize = new_filesize - cryptoff
    if cryptsize <= 0 or cryptsize % 0x1000:
        raise SystemExit("cryptsize is not page aligned")
    off = 32
    found = False
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == 0x2C:
            struct.pack_into("<III", data, off + 8, cryptoff, cryptsize, 0)
            found = True
        off += cmdsize
    if not found:
        raise SystemExit("dylib has no encryption info")


def fix_dylib(path: str) -> None:
    data = bytearray(open(path, "rb").read())
    if b"/usr/lib/libobjc.A.dylib" not in data:
        raw = b"/usr/lib/libobjc.A.dylib\x00"
        cmdsize = (24 + len(raw) + 7) & ~7
        blob = struct.pack("<IIIIII", 0xC, cmdsize, 24, 0, 0x00010000, 0x00010000) + raw
        blob = blob.ljust(cmdsize, b"\x00")
        ncmds, sizeofcmds = struct.unpack_from("<II", data, 16)
        header_end = 32 + sizeofcmds
        if header_end + cmdsize > first_section_offset(data):
            raise SystemExit("no room to link libobjc")
        data[header_end : header_end + cmdsize] = blob
        struct.pack_into("<II", data, 16, ncmds + 1, sizeofcmds + cmdsize)
    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 32
    patched = 0
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd in (0x22, 0x80000022):
            bind_off, bind_size, _, _, lazy_off, lazy_size = struct.unpack_from("<IIIIII", data, off + 16)
            patched += patch_bind_blob(data, bind_off, bind_size)
            patched += patch_bind_blob(data, lazy_off, lazy_size)
        off += cmdsize
    if patched < 10 and not (patched == 0 and b"/usr/lib/libobjc.A.dylib" in data):
        raise SystemExit("dylib bind table was not rewritten (%d)" % patched)
    normalize_for_signing(data)
    open(path, "wb").write(data)
    print("normalized dylib for signing, rewrote", patched, "binds")


def main() -> None:
    if not os.path.exists(DYLIB):
        raise SystemExit("missing " + DYLIB)
    fix_dylib(DYLIB)
    app = os.path.join(SRC, "Payload", "Instagram.app")
    with open(os.path.join(app, "Instagram"), "rb") as f:
        binary = patch_binary(f.read())
    with open(os.path.join(app, "Info.plist"), "rb") as f:
        info = plistlib.load(f)

    info["CFBundleDisplayName"] = "Instagram"
    info["CFBundleName"] = "Instagram"
    for key, files in (
        ("CFBundleIcons", ["Instagram60x60"]),
        ("CFBundleIcons~ipad", ["Instagram60x60", "Instagram76x76"]),
    ):
        primary = info[key]["CFBundlePrimaryIcon"]
        primary.pop("CFBundleIconName", None)
        primary["CFBundleIconFiles"] = files
    info_bytes = plistlib.dumps(info, fmt=plistlib.FMT_BINARY)

    icon_src = Image.open(os.path.join(app, "Prod60x60@2x.png"))
    replacements = {
        "Instagram60x60@2x.png": grayscale(icon_src, 120),
        "Instagram60x60@3x.png": grayscale(icon_src, 180),
        "Instagram76x76@2x~ipad.png": grayscale(icon_src, 152),
        "Instagram83.5@2x~ipad.png": grayscale(icon_src, 167),
        "Prod60x60@2x.png": grayscale(icon_src, 120),
        "Prod76x76@2x~ipad.png": grayscale(icon_src, 152),
        "Icon-60-Prod@2x.png": grayscale(icon_src, 120),
        "AeromortoSplash.png": splash_text(600),
    }
    for name, size in METAL.items():
        replacements[name] = splash_text(size)
    for scale, suffix in ((1, ""), (2, "@2x"), (3, "@3x")):
        replacements["aeromorto-m%s.png" % suffix] = splash_text(120 * scale)
        replacements["aerom-logo1%s.png" % suffix] = splash_text(251 * scale)

    payload_root = os.path.join(SRC, "Payload")
    for dirpath, _dirs, files in os.walk(os.path.join(payload_root, "Instagram.app")):
        for name in files:
            if not name.startswith(PULSE_PREFIX) or not name.endswith(".png"):
                continue
            full = os.path.join(dirpath, name)
            with Image.open(full) as im:
                replacements[name] = Image.new("RGBA", im.size, (0, 0, 0, 0))

    files_out = {name: png_bytes(im) for name, im in replacements.items()}

    if os.path.exists(OUT):
        os.remove(OUT)

    count = 0
    with zipfile.ZipFile(OUT, "w") as zf:
        for dirpath, _dirs, files in os.walk(payload_root):
            for name in files:
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, SRC).replace("\\", "/")
                if rel == "Payload/Instagram.app/InstagramMod.dylib":
                    continue  # o dylib novo entra no fim; evita entrada duplicada no zip
                if rel == "Payload/Instagram.app/Instagram":
                    data = binary
                elif rel == "Payload/Instagram.app/Info.plist":
                    data = info_bytes
                elif rel.startswith("Payload/Instagram.app/") and name in files_out:
                    data = files_out[name]
                else:
                    with open(full, "rb") as f:
                        data = f.read()
                compress = zipfile.ZIP_STORED if len(data) > 1024 * 1024 else zipfile.ZIP_DEFLATED
                zf.writestr(rel, data, compress_type=compress)
                count += 1
                if count % 400 == 0:
                    print("packed", count)
        for name, data in files_out.items():
            rel = "Payload/Instagram.app/" + name
            if name.startswith("Prod") or name.startswith("Icon-60") or name.startswith("Metal") or name.startswith(PULSE_PREFIX):
                continue
            zf.writestr(rel, data, compress_type=zipfile.ZIP_DEFLATED)
        zf.write(DYLIB, "Payload/Instagram.app/InstagramMod.dylib", compress_type=zipfile.ZIP_STORED)
    print("wrote", OUT, "files", count, "bytes", os.path.getsize(OUT))
    verify(OUT)


def verify(path: str) -> None:
    with zipfile.ZipFile(path) as zf:
        names = zf.namelist()
        needed = [
            "Payload/Instagram.app/Instagram",
            "Payload/Instagram.app/InstagramMod.dylib",
            "Payload/Instagram.app/Info.plist",
            "Payload/Instagram.app/Instagram60x60@2x.png",
            "Payload/Instagram.app/AeromortoSplash.png",
            "Payload/Instagram.app/Metal@3x.png",
            "Payload/Instagram.app/aeromorto-m@3x.png",
            "Payload/Instagram.app/aerom-logo1@3x.png",
        ]
        for name in needed:
            if name not in names:
                raise SystemExit("missing " + name)
        data = zf.read("Payload/Instagram.app/Instagram")
        ncmds, sizeofcmds = struct.unpack_from("<II", data, 16)
        off = 32
        found = False
        for _ in range(ncmds):
            cmd, cmdsize = struct.unpack_from("<II", data, off)
            if cmd == LC_LOAD_DYLIB:
                name_off = struct.unpack_from("<I", data, off + 8)[0]
                raw = data[off + name_off : off + cmdsize].split(b"\x00", 1)[0]
                if raw == LOAD_PATH:
                    found = True
            off += cmdsize
        if not found:
            raise SystemExit("LC_LOAD_DYLIB was not written")
        info = plistlib.loads(zf.read("Payload/Instagram.app/Info.plist"))
        primary = info["CFBundleIcons"]["CFBundlePrimaryIcon"]
        if "CFBundleIconName" in primary:
            raise SystemExit("asset-catalog icon name still set")
        if primary["CFBundleIconFiles"] != ["Instagram60x60"]:
            raise SystemExit("unexpected icon files")
        if info["CFBundleDisplayName"] != "Instagram":
            raise SystemExit("display name was not set")
        cryptid = None
        off = 32
        for _ in range(ncmds):
            cmd, cmdsize = struct.unpack_from("<II", data, off)
            if cmd == 0x2C:
                cryptid = struct.unpack_from("<I", data, off + 16)[0]
            off += cmdsize
        if cryptid != 0:
            raise SystemExit("binary is still encrypted")
        nib = zf.read("Payload/Instagram.app/IGLaunchScreen.storyboardc/MmX-nZ-UTa-view-5K5-hL-BWV.nib")
        if b"logo-splash" not in nib or b"fb-branding" not in nib:
            raise SystemExit("launch storyboard was modified")
        dylib = zf.read("Payload/Instagram.app/InstagramMod.dylib")
        if b"/usr/lib/libobjc.A.dylib" not in dylib:
            raise SystemExit("dylib is not linked to libobjc")
        if not dylib_layout_ok(dylib):
            raise SystemExit("dylib is not page-aligned for signing")
        if b"objc_copyClassList" in dylib:
            raise SystemExit("dylib still contains the crashing hooks")
        if b"modded by Aeromorto" not in dylib or b"fb-branding" not in dylib:
            raise SystemExit("dylib is missing the splash strings")
        if not dylib_minos_ok(dylib):
            raise SystemExit("dylib min OS is not 16.3")
    print("verified load command, icon, splash and display name")


def dylib_layout_ok(data: bytes) -> bool:
    if data[:4] != b"\xcf\xfa\xed\xfe":
        return False
    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 32
    text_vmsize = None
    cryptsize = None
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == 0xE:
            return False
        if cmd == 0x19:
            name = bytes(data[off + 8 : off + 24]).split(b"\x00", 1)[0]
            if name == b"__TEXT":
                text_vmsize = struct.unpack_from("<Q", data, off + 32)[0]
                filesize = struct.unpack_from("<Q", data, off + 48)[0]
                if text_vmsize % 0x1000 or filesize % 0x1000:
                    return False
        if cmd == 0x2C:
            cryptoff, cryptsize, cryptid = struct.unpack_from("<III", data, off + 8)
            if cryptid != 0 or cryptoff != 0x4000 or cryptsize % 0x1000 or cryptsize == 0:
                return False
        off += cmdsize
    return text_vmsize is not None and cryptsize is not None


def dylib_minos_ok(data: bytes) -> bool:
    if data[:4] != b"\xcf\xfa\xed\xfe":
        return False
    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == 0x32 and cmdsize >= 24:
            platform, minos = struct.unpack_from("<II", data, off + 8)
            if platform == 2 and minos == 0x00100300:
                return True
        if cmd == 0x25 and cmdsize >= 16:
            version = struct.unpack_from("<I", data, off + 8)[0]
            if version == 0x00100300:
                return True
        off += cmdsize
    return False


if __name__ == "__main__":
    main()

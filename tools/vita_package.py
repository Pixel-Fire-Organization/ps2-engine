#!/usr/bin/env python3
"""Vita package config reader, validator and emitter.

Reads game/platform/vita/package.json - the title's identity, store-front
presentation and achievements - and emits the pieces the build needs:

  * the argument lists the CMake fragment feeds to the packaging tools
  * a store-front layout template, when the config does not supply one
  * the trophy configuration files and the trophy pack
  * a C++ header of trophy identifiers, so no trophy number is written by hand

Validation is fail-loud, for one concrete reason: every mistake this catches is
otherwise silent until the console refuses to install the package, and the error
it gives then names nothing. A wrong icon size must fail here, with the file name
and the expected size, or it costs someone an afternoon.

jsonschema (draft-07) is used as an extra structural pass when importable; the
hand-rolled checks below are the source of truth and need no third-party package.

Usage:
    python3 tools/vita_package.py --validate --variant vita
    python3 tools/vita_package.py --variant vitatv --emit-cmake build/vitatv.cmake
    python3 tools/vita_package.py --emit-ids build/generated/TrophyIds.h
"""

import argparse
import hashlib
import json
import os
import struct
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_CONFIG = os.path.join(PROJECT_ROOT, "game", "platform", "vita", "package.json")
DEFAULT_SCHEMA = os.path.join(PROJECT_ROOT, "game", "platform", "package.schema.json")

ACHV_MAX_ENTRIES = 128

PNG_COLOUR_INDEXED = 3

LIVEAREA_ASSETS = {
    "icon": (128, 128, True, False),
    "background": (840, 500, True, False),
    "startup": (280, 158, True, True),
    "picture": (960, 544, False, False),
}

TROPHY_GRADES = ("platinum", "gold", "silver", "bronze")


class PackageError(Exception):
    """A configuration error with an actionable message."""

    def __init__(self, where, message):
        super().__init__(f"{where}: {message}")
        self.where = where
        self.message = message


def load_config(path):
    if not os.path.exists(path):
        raise PackageError(os.path.basename(path), f"no package config at {path}")
    with open(path, "r", encoding="utf-8") as fh:
        try:
            return json.load(fh)
        except json.JSONDecodeError as exc:
            raise PackageError(os.path.basename(path), f"invalid JSON: {exc}") from exc


def deep_merge(base, override):
    """Recursive merge. Dicts merge key-wise; lists and scalars replace outright.

    Replacing rather than appending is deliberate: a variant that overrides the
    trophy list means to supply a different list, not to extend the base one.
    """
    out = dict(base)
    for key, value in override.items():
        if key in out and isinstance(out[key], dict) and isinstance(value, dict):
            out[key] = deep_merge(out[key], value)
        else:
            out[key] = value
    return out


def resolve_variant(config, variant):
    """Apply the variant override block, and drop 'variants' from the result."""
    resolved = dict(config)
    variants = resolved.pop("variants", {})
    if variant and variant in variants:
        resolved = deep_merge(resolved, variants[variant])
        resolved.pop("variants", None)
    return resolved


def schema_validate(data, schema_path):
    """Optional draft-07 pass; silently skipped if jsonschema is unavailable."""
    try:
        import jsonschema  # type: ignore
    except ImportError:
        return
    if not os.path.exists(schema_path):
        return
    with open(schema_path, "r", encoding="utf-8") as fh:
        schema = json.load(fh)
    validator = jsonschema.Draft7Validator(schema)
    errors = sorted(validator.iter_errors(data), key=lambda e: list(e.path))
    if errors:
        parts = []
        for err in errors:
            loc = "/".join(str(p) for p in err.path) or "<root>"
            parts.append(f"  {loc}: {err.message}")
        raise PackageError("schema", "draft-07 validation failed:\n" + "\n".join(parts))


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def png_info(path):
    """Return (width, height, bit_depth, colour_type, has_transparency)."""
    with open(path, "rb") as fh:
        if fh.read(8) != PNG_SIGNATURE:
            raise PackageError(os.path.basename(path), "not a PNG file")
        length = struct.unpack(">I", fh.read(4))[0]
        if fh.read(4) != b"IHDR" or length != 13:
            raise PackageError(os.path.basename(path), "malformed PNG: no IHDR")
        width, height, depth, colour = struct.unpack(">IIBB", fh.read(10))
        fh.read(3 + 4)  # rest of IHDR, then its CRC

        has_alpha = colour in (4, 6)
        while not has_alpha:
            head = fh.read(8)
            if len(head) < 8:
                break
            clen, ctype = struct.unpack(">I4s", head)
            if ctype == b"tRNS":
                has_alpha = True
                break
            if ctype == b"IDAT":
                break  # tRNS must precede IDAT, so there is none
            fh.seek(clen + 4, os.SEEK_CUR)
    return width, height, depth, colour, has_alpha


def _resolve(config_dir, rel):
    return rel if os.path.isabs(rel) else os.path.normpath(os.path.join(config_dir, rel))


def validate(config, config_dir, variant=None):
    """Raise PackageError on the first structural problem; return the config."""
    where = "package.json" + (f" [{variant}]" if variant else "")

    title = config.get("title") or {}
    tid = title.get("id", "")
    if len(tid) != 9 or not tid[:4].isalpha() or not tid[:4].isupper() or not tid[4:].isdigit():
        raise PackageError(where, f"title.id '{tid}' must be four upper-case letters then five digits")

    version = title.get("version", "")
    if len(version) != 5 or version[2] != "." or not (version[:2] + version[3:]).isdigit():
        raise PackageError(where, f"title.version '{version}' must be ##.##")

    if not title.get("name"):
        raise PackageError(where, "title.name is required and must not be empty")

    _validate_livearea(config, config_dir, where)
    _validate_trophies(config, config_dir, where)
    _validate_files(config, config_dir, where)
    return config


def _validate_livearea(config, config_dir, where):
    livearea = config.get("livearea")
    if not livearea:
        return

    for key, (want_w, want_h, required, alpha_ok) in LIVEAREA_ASSETS.items():
        rel = livearea.get(key)
        if not rel:
            if required:
                raise PackageError(where, f"livearea.{key} is required when a store-front is declared")
            continue

        path = _resolve(config_dir, rel)
        if not os.path.exists(path):
            raise PackageError(where, f"livearea.{key} points at a missing file: {rel}")

        width, height, depth, colour, has_alpha = png_info(path)
        if (width, height) != (want_w, want_h):
            raise PackageError(
                where,
                f"livearea.{key} ({rel}) is {width}x{height}, must be exactly {want_w}x{want_h}. "
                "The console rejects the whole package on a wrong size and names nothing.")
        if colour != PNG_COLOUR_INDEXED:
            raise PackageError(
                where,
                f"livearea.{key} ({rel}) has PNG colour type {colour}, must be {PNG_COLOUR_INDEXED} (indexed). "
                "Convert it to an indexed palette; a plain RGB image installs as an error code.")
        if depth > 8:
            raise PackageError(where, f"livearea.{key} ({rel}) is {depth}-bit, must be 8-bit or less")
        if has_alpha and not alpha_ok:
            raise PackageError(where, f"livearea.{key} ({rel}) has transparency, which is not allowed for this layer")

    template = livearea.get("template")
    if template and not os.path.exists(_resolve(config_dir, template)):
        raise PackageError(where, f"livearea.template points at a missing file: {template}")


def _validate_trophies(config, config_dir, where):
    trophies = config.get("trophies")
    if not trophies:
        return

    has_trp = bool(trophies.get("trp"))
    entries = trophies.get("list") or []
    enabled = bool(trophies.get("enabled"))

    if has_trp and entries:
        raise PackageError(where, "trophies.trp and trophies.list are mutually exclusive - supply a pre-built pack or definitions, not both")

    if enabled:
        comm = trophies.get("np_communication_id", "")
        if not (len(comm) == 12 and comm.startswith("NPWR") and comm[4:9].isdigit() and comm.endswith("_00")):
            raise PackageError(where, f"trophies.np_communication_id '{comm}' must look like NPWR#####_00")
        if not has_trp and not entries:
            raise PackageError(where, "trophies.enabled is set but neither trophies.trp nor trophies.list was given")

    if has_trp:
        path = _resolve(config_dir, trophies["trp"])
        if not os.path.exists(path):
            raise PackageError(where, f"trophies.trp points at a missing file: {trophies['trp']}")
        return

    if not entries:
        return

    if len(entries) > ACHV_MAX_ENTRIES:
        raise PackageError(where, f"{len(entries)} trophies declared, the maximum is {ACHV_MAX_ENTRIES}")

    ids = [e.get("id") for e in entries]
    if len(set(ids)) != len(ids):
        dupes = sorted({i for i in ids if ids.count(i) > 1})
        raise PackageError(where, f"duplicate trophy ids: {dupes}")
    if sorted(ids) != list(range(len(ids))):
        raise PackageError(
            where,
            f"trophy ids must be dense and start at zero; got {sorted(ids)}. "
            "A gap means an icon the pack does not carry, which nothing can recover from at runtime.")

    platinums = [e for e in entries if e.get("grade") == "platinum"]
    if len(platinums) > 1:
        raise PackageError(where, f"{len(platinums)} platinum trophies declared, at most one is allowed")

    for entry in entries:
        if entry.get("grade") not in TROPHY_GRADES:
            raise PackageError(where, f"trophy {entry.get('id')} has grade '{entry.get('grade')}', must be one of {list(TROPHY_GRADES)}")
        icon = entry.get("icon")
        if icon and not os.path.exists(_resolve(config_dir, icon)):
            raise PackageError(where, f"trophy {entry.get('id')} icon points at a missing file: {icon}")


def _validate_files(config, config_dir, where):
    for item in config.get("files") or []:
        src = _resolve(config_dir, item["src"])
        if not os.path.exists(src):
            raise PackageError(where, f"files[] entry points at a missing file: {item['src']}")
        if item["dst"].startswith("/") or ".." in item["dst"].split("/"):
            raise PackageError(where, f"files[] destination must be a relative path inside the package: {item['dst']}")


def _sfo_args(config):
    """Metadata arguments, in a stable order so the output is reproducible."""
    title = config["title"]
    sfo = config.get("sfo") or {}
    args = [f"-s TITLE_ID={title['id']}", f"-s APP_VER={title['version']}"]
    if sfo.get("category"):
        args.append(f"-s CATEGORY={sfo['category']}")
    if "parental_level" in sfo:
        args.append(f"-d PARENTAL_LEVEL={sfo['parental_level']}")
    if (config.get("self") or {}).get("extended_memory"):
        args.append("-d ATTRIBUTE2=12")
    trophies = config.get("trophies") or {}
    if trophies.get("enabled") and trophies.get("np_communication_id"):
        args.append(f"-s NP_COMMUNICATION_ID={trophies['np_communication_id']}")
    for key in sorted((sfo.get("extra") or {}).keys()):
        value = sfo["extra"][key]
        args.append(f"-d {key}={value}" if isinstance(value, int) else f"-s {key}={value}")
    return args


def _package_files(config, config_dir, generated_dir):
    """(absolute source, destination inside the package) pairs, in stable order."""
    pairs = []
    livearea = config.get("livearea") or {}
    dests = {
        "icon": "sce_sys/icon0.png",
        "picture": "sce_sys/pic0.png",
        "background": "sce_sys/livearea/contents/bg0.png",
        "startup": "sce_sys/livearea/contents/startup.png",
    }
    for key in ("icon", "picture", "background", "startup"):
        if livearea.get(key):
            pairs.append((_resolve(config_dir, livearea[key]), dests[key]))

    if livearea:
        template = livearea.get("template")
        src = _resolve(config_dir, template) if template else os.path.join(generated_dir, "template.xml")
        pairs.append((src, "sce_sys/livearea/contents/template.xml"))

    trophies = config.get("trophies") or {}
    if trophies.get("enabled"):
        trp = trophies.get("trp")
        src = _resolve(config_dir, trp) if trp else os.path.join(generated_dir, "TROPHY.TRP")
        pairs.append((src, "sce_sys/trophy/TROPHY.TRP"))

    for item in config.get("files") or []:
        pairs.append((_resolve(config_dir, item["src"]), item["dst"]))
    return pairs


def emit_cmake(config, config_dir, generated_dir, out_path):
    """Emit set() lines the platform fragment includes.

    The root requires CMake 3.10, whose string(JSON) does not exist, so the config
    is read here and handed over as plain variables.
    """
    title = config["title"]
    self_cfg = config.get("self") or {}
    trophies = config.get("trophies") or {}

    fself = []
    if self_cfg.get("compress", True):
        fself.append("-c")
    if self_cfg.get("safe", True):
        fself.append("-s")
    if self_cfg.get("memsize"):
        fself.append(f"-m {self_cfg['memsize']}")

    lines = [
        "# Generated by tools/vita_package.py - do not edit.",
        f'set(VITA_TITLE_ID "{title["id"]}")',
        f'set(VITA_TITLE_NAME "{title["name"]}")',
        f'set(VITA_TITLE_VERSION "{title["version"]}")',
        'set(VITA_MKSFOEX_ARGS "{}")'.format(";".join(_sfo_args(config)).replace('"', '\\"')),
        'set(VITA_MAKE_FSELF_ARGS "{}")'.format(";".join(fself)),
        'set(VITA_TROPHIES_ENABLED "{}")'.format("ON" if trophies.get("enabled") else "OFF"),
        'set(VITA_NP_COMM_ID "{}")'.format(trophies.get("np_communication_id", "")),
        'set(VITA_TROPHY_GENERATE "{}")'.format("ON" if trophies.get("enabled") and not trophies.get("trp") else "OFF"),
        'set(VITA_TROPHY_COUNT "{}")'.format(len(trophies.get("list") or [])),
    ]

    pairs = _package_files(config, config_dir, generated_dir)
    flat = ";".join(f"{src}|{dst}" for src, dst in pairs)
    lines.append(f'set(VITA_PACKAGE_FILES "{flat}")')

    _write(out_path, "\n".join(lines) + "\n")
    return out_path


TEMPLATE_XML = """<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by tools/vita_package.py from livearea.style - do not edit. -->
<livearea style="{style}" format-ver="01.00" content-rev="1">
  <livearea-background>
    <image>bg0.png</image>
  </livearea-background>
  <gate>
    <startup-image>startup.png</startup-image>
  </gate>
</livearea>
"""


def emit_template(config, out_path):
    style = (config.get("livearea") or {}).get("style", "a1")
    _write(out_path, TEMPLATE_XML.format(style=style))
    return out_path


def _xml_escape(text):
    return (str(text).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            .replace('"', "&quot;").replace("'", "&apos;"))


def emit_trophy_conf(config, out_dir):
    """Write the trophy set definition and its localised strings."""
    trophies = config["trophies"]
    entries = sorted(trophies["list"], key=lambda e: e["id"])
    comm = trophies["np_communication_id"]

    conf = ['<?xml version="1.0" encoding="utf-8"?>',
            f'<trophyconf><npcommid>{comm}</npcommid><trophyset-version>01.00</trophyset-version>']
    strings = ['<?xml version="1.0" encoding="utf-8"?>',
               f'<trophyconf><npcommid>{comm}</npcommid>',
               f'<title-name>{_xml_escape(config["title"]["name"])}</title-name>']

    for entry in entries:
        hidden = "yes" if entry.get("hidden") else "no"
        conf.append(f'<trophy id="{entry["id"]:03d}" hidden="{hidden}" ttype="{entry["grade"][0].upper()}"/>')
        strings.append(
            f'<trophy id="{entry["id"]:03d}" hidden="{hidden}" ttype="{entry["grade"][0].upper()}">'
            f'<name>{_xml_escape(entry["name"])}</name>'
            f'<detail>{_xml_escape(entry.get("detail", ""))}</detail></trophy>')

    conf.append("</trophyconf>")
    strings.append("</trophyconf>")

    os.makedirs(out_dir, exist_ok=True)
    _write(os.path.join(out_dir, "TROPCONF.SFM"), "\n".join(conf) + "\n")
    _write(os.path.join(out_dir, "TROP.SFM"), "\n".join(strings) + "\n")
    return out_dir


TRP_HEADER_SIZE = 0x40
TRP_ENTRY_SIZE = 0x40
TRP_NAME_SIZE = 0x24
TRP_VERSION = 3


def build_trp(files, magic, dev_flag=0):
    """Pack (name, bytes) pairs into a trophy container.

    `magic` has no default on purpose. Neither public reader validates the field,
    so its correct value is not established, and a container written with a
    guessed one installs as an error code that names nothing. See the "Unverified"
    section of docs/formats/TROPHY_PACK.md.
    """
    count = len(files)
    data_start = TRP_HEADER_SIZE + count * TRP_ENTRY_SIZE

    entries, blobs, offset = [], [], data_start
    for name, payload in files:
        encoded = name.encode("ascii")
        if len(encoded) >= TRP_NAME_SIZE:
            raise PackageError("TROPHY.TRP", f"entry name too long: {name}")
        entries.append(encoded.ljust(TRP_NAME_SIZE, b"\0")
                       + struct.pack(">III", offset, 0, len(payload))
                       + b"\0" * 16)
        blobs.append(payload)
        offset += len(payload)

    total = offset
    header = (struct.pack(">IIQIII", magic, TRP_VERSION, total, count, TRP_ENTRY_SIZE, dev_flag)
              + b"\0" * 20 + b"\0" * 16)
    assert len(header) == TRP_HEADER_SIZE, len(header)

    body = header + b"".join(entries) + b"".join(blobs)
    digest = hashlib.sha1(body).digest()
    return body[:0x1C] + digest + body[0x30:]


def emit_trp(config, config_dir, conf_dir, out_path, magic):
    trophies = config["trophies"]
    files = []
    for name in ("TROPCONF.SFM", "TROP.SFM"):
        with open(os.path.join(conf_dir, name), "rb") as fh:
            files.append((name, fh.read()))
    for entry in sorted(trophies["list"], key=lambda e: e["id"]):
        icon = entry.get("icon")
        if not icon:
            continue
        with open(_resolve(config_dir, icon), "rb") as fh:
            files.append((f"TROP{entry['id']:03d}.PNG", fh.read()))

    _write_bytes(out_path, build_trp(files, magic))
    return out_path


def emit_ids(config, out_path):
    """A C++ header of trophy identifiers, generated from the same declaration
    the pack is built from, so the two cannot drift."""
    trophies = config.get("trophies") or {}
    entries = sorted(trophies.get("list") or [], key=lambda e: e["id"])

    lines = [
        "#pragma once",
        "",
        "// Generated by tools/vita_package.py from game/platform/vita/package.json.",
        "// Do not edit: regenerate by building.",
        "",
        "#include <cstdint>",
        "",
        "enum class TrophyId : uint8_t",
        "{",
    ]
    for entry in entries:
        ident = "".join(c if c.isalnum() else "_" for c in entry["name"]).strip("_")
        if not ident or ident[0].isdigit():
            ident = "Trophy" + ident
        detail = entry.get("detail", "")
        comment = f" // {entry['grade']}" + (f" - {detail}" if detail else "")
        lines.append(f"    {ident} = {entry['id']},{comment}")
    lines += ["", f"    Count = {len(entries)}", "};", ""]

    _write(out_path, "\n".join(lines))
    return out_path


def _write(path, text):
    parent = os.path.dirname(os.path.abspath(path))
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def _write_bytes(path, data):
    parent = os.path.dirname(os.path.abspath(path))
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "wb") as fh:
        fh.write(data)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Vita package config reader and emitter")
    ap.add_argument("--config", default=DEFAULT_CONFIG)
    ap.add_argument("--schema", default=DEFAULT_SCHEMA)
    ap.add_argument("--variant", help="variant whose override block to apply (vita, vitatv)")
    ap.add_argument("--generated-dir", default=".", help="where generated pieces are written, for path emission")
    ap.add_argument("--validate", action="store_true")
    ap.add_argument("--emit-cmake", metavar="FILE")
    ap.add_argument("--emit-template", metavar="FILE")
    ap.add_argument("--emit-trophy-conf", metavar="DIR")
    ap.add_argument("--emit-trp", metavar="FILE")
    ap.add_argument("--emit-ids", metavar="FILE")
    ap.add_argument("--trp-magic", help="container magic, e.g. 0xDCA24D00. Required by --emit-trp; see docs/formats/TROPHY_PACK.md")
    args = ap.parse_args(argv)

    config_dir = os.path.dirname(os.path.abspath(args.config))

    try:
        raw = load_config(args.config)
        schema_validate(raw, args.schema)
        config = resolve_variant(raw, args.variant)
        validate(config, config_dir, args.variant)

        did_something = args.validate
        if args.emit_template:
            print("template  ->", emit_template(config, args.emit_template))
            did_something = True
        if args.emit_trophy_conf:
            if (config.get("trophies") or {}).get("list"):
                print("trophy conf ->", emit_trophy_conf(config, args.emit_trophy_conf))
            did_something = True
        if args.emit_trp:
            if not args.trp_magic:
                raise PackageError(
                    "--emit-trp",
                    "needs --trp-magic. The container magic is not established by any public reader, "
                    "and a guessed value installs as an error code that names nothing. Take the first "
                    "four bytes of any genuine TROPHY.TRP, or set trophies.trp to a pre-built pack "
                    "instead. See docs/formats/TROPHY_PACK.md.")
            conf_dir = args.emit_trophy_conf or os.path.dirname(os.path.abspath(args.emit_trp))
            print("trophy pack ->", emit_trp(config, config_dir, conf_dir, args.emit_trp, int(args.trp_magic, 0)))
            did_something = True
        if args.emit_ids:
            print("trophy ids ->", emit_ids(config, args.emit_ids))
            did_something = True
        if args.emit_cmake:
            print("cmake     ->", emit_cmake(config, config_dir, args.generated_dir, args.emit_cmake))
            did_something = True

        if args.validate:
            name = config["title"]["name"]
            print(f"vita_package: OK - {config['title']['id']} '{name}' {config['title']['version']}"
                  + (f" [{args.variant}]" if args.variant else ""))
        if not did_something:
            ap.error("nothing to do: pass --validate or one of the --emit-* options")
    except PackageError as exc:
        print(f"vita_package: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

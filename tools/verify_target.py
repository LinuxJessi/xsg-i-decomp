#!/usr/bin/env python3
"""Verify local target pins, never create or update them. Python >= 3.10.

verify_target(manifest_path=DEFAULT_MANIFEST, game_dir=DEFAULT_GAME_DIR) returns
a JSON-compatible dict with ok=True, manifest_sha256, schema_version, revision,
provenance, units, system_cnf, and section_aliases. The last six fields have the
manifest schema. Units are keyed by main/ov01/ov02/ov10/ov11/ov12; header,
sections, and program_headers use tools.inventory.ELF metadata, with explicit
header class/endian added. VAs are lowercase 0x + eight hex digits; offsets,
sizes, indices and flags are integers. NOBITS/NULL offsets and hashes are null.
Alias placements include every matching main/OV02 section and every covering
PT_LOAD, with the section's physical address derived from that segment.

All mismatches, missing inputs, and malformed inputs raise TargetError
(ValueError); no partial success metadata is returned. Defaults are relative
to this module's project root, not cwd. Explicit relative paths use cwd.
Success attests only to these local pins, not external ISO/retail authenticity,
compiler identity, pointer widths, runtime loading behavior, or matched C.
CLI: JSON summary on stdout; exit 0 on success, 1 on verification failure.
"""

import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.inventory import ELF, INPUTS, sha256, va


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = PROJECT_ROOT / "config/targets.json"
DEFAULT_GAME_DIR = PROJECT_ROOT / "game/extracted-iso"
UNIT_PATHS = {unit: filename for unit, filename, _ in INPUTS}
PROVENANCE = {
    "source": "extracted local files",
    "source_root": "game/extracted-iso",
    "external_iso_authentication": "not_performed",
    "retail_revision_authenticated": False,
}


class TargetError(ValueError):
    """Missing, malformed, or nonmatching local target evidence."""


def _object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise TargetError(f"duplicate manifest key: {key}")
        result[key] = value
    return result


def _keys(value, keys, context):
    if not isinstance(value, dict) or set(value) != set(keys):
        raise TargetError(f"{context}: expected exactly {', '.join(sorted(keys))}")


def _equal(expected, actual, context):
    # JSON comparison also rejects bool/float substitutes for integer pins.
    if json.dumps(expected, sort_keys=True, allow_nan=False) != json.dumps(
            actual, sort_keys=True, allow_nan=False):
        raise TargetError(f"{context}: mismatch or malformed metadata")


def _describe_elf(elf, filename):
    return dict(path=filename, size=len(elf.data), sha256=sha256(elf.data),
                header=dict(elf.header, **{"class": "ELF32", "endian": "little"}),
                sections=elf.sections, program_headers=elf.program_headers)


def _section_aliases(elfs):
    aliases = []
    main, canonical = elfs["main"], elfs["ov02"]
    for source in main.sections:
        if not source["flags"] & 2 or source["type"] in (0, 8) or not source["size"]:
            continue
        matches = [s for s in canonical.sections
                   if s["flags"] & 2 and s["type"] not in (0, 8)
                   and s["va"] == source["va"] and s["size"] == source["size"]
                   and main.section_bytes(source) == canonical.section_bytes(s)]
        if len(matches) > 1:
            raise TargetError("ambiguous embedded OV02 section alias")
        if not matches:
            continue
        target = matches[0]
        placements = []
        for elf, section in ((main, source), (canonical, target)):
            loads = []
            for segment in elf.program_headers:
                delta = section["file_offset"] - segment["file_offset"]
                if (segment["type"] == 1 and delta >= 0
                        and delta + section["size"] <= segment["file_size"]
                        and int(segment["va"], 16) + delta == int(section["va"], 16)):
                    physical = int(segment["physical_address"], 16) + delta
                    if physical + section["size"] > 0x100000000:
                        raise TargetError("alias physical extent overflows ELF32")
                    loads.append(dict(index=segment["index"], physical_address=va(physical)))
            placements.append(dict(unit=elf.unit, section_index=section["index"],
                                   name=section["name"], file_offset=section["file_offset"],
                                   load_segments=loads))
        aliases.append(dict(canonical_unit="ov02", canonical_section=target["index"],
                            va=source["va"], size=source["size"], sha256=source["sha256"],
                            proof="equal_va_size_and_section_bytes", placements=placements))
    if not aliases:
        raise TargetError("missing embedded OV02 byte/VA/size alias proof")
    return aliases


def verify_target(manifest_path=DEFAULT_MANIFEST, game_dir=DEFAULT_GAME_DIR):
    """Read and verify all seven files; return pinned metadata or raise TargetError.

    Manifest v1 is closed: unknown/missing keys, duplicate JSON keys, omitted
    sections/segments/aliases, and wrong numeric types or hex/hash values fail.
    Paths must be the fixed basenames in UNIT_PATHS or SYSTEM.CNF, and their
    resolved paths must stay under game_dir (including symlink resolution).
    """
    try:
        raw = Path(manifest_path).read_bytes()
        manifest = json.loads(raw.decode("utf-8"), object_pairs_hook=_object)
        _keys(manifest, ("schema_version", "revision", "provenance", "units",
                         "system_cnf", "section_aliases"), "manifest")
        _equal(manifest["schema_version"], 1, "schema_version")
        _equal(manifest["provenance"], PROVENANCE, "provenance")
        _keys(manifest["units"], UNIT_PATHS, "units")
        root = Path(game_dir).resolve()
        elfs = {}
        for unit, filename in [*UNIT_PATHS.items(), ("system_cnf", "SYSTEM.CNF")]:
            record = manifest["system_cnf"] if unit == "system_cnf" else manifest["units"][unit]
            fields = ("observations",) if unit == "system_cnf" else ("header", "sections", "program_headers")
            _keys(record, ("path", "size", "sha256", *fields), unit)
            _equal(record["path"], filename, f"{unit}.path")
            if type(record["size"]) is not int or record["size"] <= 0:
                raise TargetError(f"{unit}.size: expected a positive integer")
            if not isinstance(record["sha256"], str) or not re.fullmatch("[0-9a-f]{64}", record["sha256"]):
                raise TargetError(f"{unit}.sha256: expected 64 lowercase hex digits")
            path = (root / filename).resolve()
            if not path.is_relative_to(root):
                raise TargetError(f"{unit}.path: escapes game_dir")
            data = path.read_bytes()
            _equal(record["size"], len(data), f"{unit}.size")
            _equal(record["sha256"], sha256(data), f"{unit}.sha256")
            if unit == "system_cnf":
                observations = {}
                for line in data.decode("ascii").splitlines():
                    if not line.strip():
                        continue
                    key, separator, value = line.partition("=")
                    key, value = key.strip(), value.strip()
                    if not separator or not key or not value or key in observations:
                        raise TargetError("SYSTEM.CNF: malformed or duplicate assignment")
                    observations[key] = value
                if not {"BOOT2", "VER", "VMODE"} <= observations.keys():
                    raise TargetError("SYSTEM.CNF: missing boot observations")
                _equal(record["observations"], observations, "system_cnf.observations")
            else:
                elfs[unit] = ELF(data, unit)
                _equal(record, _describe_elf(elfs[unit], filename), unit)
        _equal(manifest["revision"], "slus-20469-" + manifest["units"]["main"]["sha256"][:12], "revision")
        _equal(manifest["section_aliases"], _section_aliases(elfs), "section_aliases")
        return dict(manifest, ok=True, manifest_sha256=sha256(raw))
    except (OSError, ValueError, RuntimeError) as exc:
        raise TargetError(str(exc)) from exc


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--game-dir", type=Path, default=DEFAULT_GAME_DIR)
    args = parser.parse_args(argv)
    try:
        result = verify_target(args.manifest, args.game_dir)
        summary = {key: result[key] for key in ("ok", "manifest_sha256", "revision", "provenance")}
        summary["units"] = {unit: {key: record[key] for key in ("path", "size", "sha256")}
                            for unit, record in result["units"].items()}
        summary["system_cnf"] = result["system_cnf"]
        summary["section_alias_count"] = len(result["section_aliases"])
        print(json.dumps(summary, sort_keys=True))
        return 0
    except TargetError as exc:
        print(json.dumps(dict(ok=False, error=str(exc)), sort_keys=True))
        return 1


if __name__ == "__main__":
    sys.exit(main())

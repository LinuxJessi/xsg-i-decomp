#!/usr/bin/env python3
"""Read-only target/compiler evidence, not compiler selection or matching.

Python >= 3.10, stdlib only. First verify_target checks the local originals;
the reviewed manifest hash is additionally fixed here. No executable is probed
or run. --output writes only the requested JSON, never its parent directories.
Defaults are module-relative, not cwd-relative. In an external sandbox, put the
project on PYTHONPATH; --root selects its target inputs, not a compiler payload.
Instruction observations are narrow bit-field checks, not a full EE decoder.
"""

import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import sys

sys.dont_write_bytecode = True
if __package__ in (None, ""):
    for parent in Path(__file__).resolve().parents:
        if (parent / "tools/inventory.py").is_file():
            sys.path.insert(0, str(parent))
            break
from tools.inventory import ELF, sha256, va
from tools.verify_target import PROJECT_ROOT, verify_target


TARGET_MANIFEST_SHA256 = "f38aeb224e2d96ababcdce03c4aa4461ce6da22de8feffb86425ee332501a7e3"
DEFAULT_CALIBRATION = Path(__file__).resolve().parents[1] / "config/calibration.json"
ANALYSIS_REVISION = "B01-byte-features-v1"
RUNTIME_NAMES = (
    "__main", "__muldi3", "__divdi3", "__moddi3", "__udivdi3", "__umoddi3",
    "__register_frame", "__register_frame_info", "__deregister_frame",
    "malloc", "memcpy", "sceSifInitRpc",
)
MEMORY_OPS = {32: "lb", 33: "lh", 35: "lw", 36: "lbu", 37: "lhu",
              39: "lwu", 40: "sb", 41: "sh", 43: "sw", 49: "lwc1",
              55: "ld", 57: "swc1", 63: "sd"}


def instruction_features(data, address):
    """Only classify explicitly supported encodings; retain the raw witness word."""
    if address % 4 or not data or len(data) % 4:
        raise ValueError("expected a nonempty, word-aligned function extent")
    result = []
    for index, (word,) in enumerate(struct.iter_unpack("<I", data)):
        pc = address + index * 4
        op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
        imm = (word & 65535) - (65536 if word & 32768 else 0)
        features = []
        if op == 3:
            features.append(("jal", dict(target_va=va(((pc + 4) & 0xf0000000) | ((word & 0x3ffffff) << 2)))))
        if word & 0xfc1fffff == 8:
            features.append(("jr", dict(register=rs, is_return=rs == 31)))
        if op in (20, 21) or (op in (22, 23) and rt == 0):
            features.append(("branch_likely", dict(target_va=va(pc + 4 + imm * 4))))
        if op in (9, 25) and rs == rt == 29:
            features.append(("stack_adjust", dict(mnemonic="addiu" if op == 9 else "daddiu", delta=imm)))
        if op in MEMORY_OPS and rs == 28:
            features.append(("gp_relative_memory", dict(mnemonic=MEMORY_OPS[op], displacement=imm, register=rt)))
        if op in (55, 63):
            features.append(("gpr_64bit_memory", dict(mnemonic=MEMORY_OPS[op], base_register=rs, register=rt, displacement=imm)))
        if op in (17, 18):
            features.append(("cop1_opcode" if op == 17 else "cop2_opcode", {}))
        if op in (49, 57):
            features.append(("fpr_32bit_memory", dict(mnemonic=MEMORY_OPS[op], base_register=rs, register=rt, displacement=imm)))
        for kind, fields in features:
            result.append(dict(kind=kind, va=va(pc), word=f"0x{word:08x}", **fields))
    return result


def function_record(elf, address):
    symbols = [s for s in elf.function_symbols if s["va"] == address]
    if not symbols:
        raise ValueError(f"{elf.unit}:{address}: no file-backed FUNC symbol")
    if len({(s["size"], s["sha256"], s["file_offset"]) for s in symbols}) != 1:
        raise ValueError(f"{elf.unit}:{address}: conflicting FUNC extents")
    first = symbols[0]
    data = elf.span(first["file_offset"], first["size"], first["name"])
    names = list(dict.fromkeys(s["name"] for s in symbols))
    return dict(unit=elf.unit, va=address, original_name=first["name"], aliases=names[1:],
                symbol_ids=[s["id"] for s in symbols], size=len(data), sha256=sha256(data),
                section_index=first["section_index"], file_offset=first["file_offset"],
                observed_instruction_features=instruction_features(data, int(address, 16)))


def data_record(elf, address, size):
    if type(size) is not int or not 0 < size <= 4096:
        raise ValueError("data context must be 1..4096 bytes")
    start = int(address, 16)
    sections = [s for s in elf.sections if s["flags"] & 2 and s["type"] not in (0, 8)
                and int(s["va"], 16) <= start and start + size <= int(s["va"], 16) + s["size"]]
    if len(sections) != 1:
        raise ValueError(f"{elf.unit}:{address}: ambiguous or unavailable data context")
    section = sections[0]
    offset = section["file_offset"] + start - int(section["va"], 16)
    data = elf.span(offset, size, "calibration data")
    return dict(va=address, size=size, section_index=section["index"], file_offset=offset, sha256=sha256(data))


def unit_evidence(elf, pin):
    markers = [s for s in elf.symbols if s["name"].startswith("gcc2_compiled")]
    fields = ("id", "name", "va", "size", "type", "binding", "section_index")
    debug = [s for s in elf.sections if s["name"].startswith((".debug", ".zdebug", ".mdebug", ".stab"))]
    return dict(path=pin["path"], sha256=pin["sha256"], header=pin["header"],
                gcc2_marker_count=len(markers),
                gcc2_markers_by_section=dict(sorted(Counter(s["section"] for s in markers).items())),
                gcc2_markers=[{k: s[k] for k in fields} for s in markers],
                runtime_name_filter=list(RUNTIME_NAMES),
                runtime_symbols=[{k: s[k] for k in (*fields, "sha256")} for s in elf.symbols if s["name"] in RUNTIME_NAMES],
                debug_sections=debug, nonempty_debug_sections_present=any(s["size"] for s in debug),
                comment_sections=[s for s in elf.sections if s["name"] == ".comment"],
                small_data_sections=[s for s in elf.sections if s["name"] in (".sdata", ".sbss", ".lit4", ".lit8")])


def collect(root=PROJECT_ROOT, calibration_path=DEFAULT_CALIBRATION):
    root = Path(root).resolve()
    target = verify_target(root / "config/targets.json", root / "game/extracted-iso")
    if target["manifest_sha256"] != TARGET_MANIFEST_SHA256:
        raise ValueError("target manifest differs from reviewed B00 pin")
    raw = Path(calibration_path).read_bytes()
    corpus = json.loads(raw)
    if (corpus["schema_version"] != 1 or corpus["analysis_revision"] != ANALYSIS_REVISION
            or corpus["target_manifest_sha256"] != TARGET_MANIFEST_SHA256
            or corpus["revision"] != target["revision"]):
        raise ValueError("calibration schema, analysis, or target identity mismatch")
    if not 10 <= len(corpus["functions"]) <= 20:
        raise ValueError("calibration must contain 10..20 functions")
    elfs, units = {}, {}
    for unit, pin in target["units"].items():
        data = (root / "game/extracted-iso" / pin["path"]).read_bytes()
        if sha256(data) != pin["sha256"]:
            raise ValueError(f"{unit}: original changed after target verification")
        elfs[unit] = ELF(data, unit)
        units[unit] = unit_evidence(elfs[unit], pin)
    seen = set()
    for entry in corpus["functions"]:
        unit, address = entry["unit"], entry["va"]
        if (unit, address) in seen:
            raise ValueError("duplicate calibration identity")
        seen.add((unit, address))
        if entry["binary_sha256"] != units[unit]["sha256"]:
            raise ValueError(f"{unit}:{address}: binary identity mismatch")
        actual = function_record(elfs[unit], address)
        for key, value in actual.items():
            if entry[key] != value:
                raise ValueError(f"{unit}:{address}: {key} mismatch")
        for context in entry.get("data_context", []):
            actual_data = data_record(elfs[unit], context["va"], context["size"])
            for key, value in actual_data.items():
                if context[key] != value:
                    raise ValueError(f"{unit}:{address}: data context {key} mismatch")
            if "target_vas" in context:
                data = elfs[unit].span(actual_data["file_offset"], actual_data["size"], "dispatch table")
                if len(data) % 4 or context["target_unit"] != unit or context["target_vas"] != [
                        va(w[0]) for w in struct.iter_unpack("<I", data)]:
                    raise ValueError(f"{unit}:{address}: dispatch table targets mismatch")
        embedded = []
        if unit == "ov02":
            source_sections = {p["section_index"] for a in target["section_aliases"]
                               if a["canonical_unit"] == unit and a["canonical_section"] == actual["section_index"]
                               for p in a["placements"] if p["unit"] == "main"}
            embedded = [s for s in elfs["main"].function_symbols
                        if s["va"] == address and s["section_index"] in source_sections]
        if entry.get("embedded_alias_symbols", []) != embedded:
            raise ValueError(f"{unit}:{address}: embedded aliases mismatch")
    return dict(schema_version=1, ok=True, evidence_level="verified_local_identity_and_instruction_fields",
                toolchain_status="blocked", gate_passed=False, selected_compiler=None, matched_c_functions=0,
                target_manifest_sha256=TARGET_MANIFEST_SHA256, revision=target["revision"],
                analysis_revision=ANALYSIS_REVISION, provenance=target["provenance"], units=units,
                calibration_sha256=sha256(raw), calibration_functions_verified=len(seen),
                limitations=["Symbol boundaries are not proven source TU boundaries.",
                             "Markers and runtime names do not identify exact compiler versions.",
                             "No type model, flags, EE decoder, compiler, assembler, or linker is validated.",
                             "Rationale and semantic notes are reviewed analysis, not assertions checked by this collector."])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=PROJECT_ROOT)
    parser.add_argument("--calibration", type=Path, default=DEFAULT_CALIBRATION)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        result = collect(args.root, args.calibration)
        text = json.dumps(result, indent=2, sort_keys=True) + "\n"
        if args.output:
            root = args.root.resolve()
            protected = [root / "game", root / "game/extracted-iso", root / "config",
                         root / "config/targets.json", args.calibration.absolute()]
            # Protect input symlink destinations as well as their lexical aliases.
            protected += [path.resolve() for path in protected]
            outputs = (args.output.absolute(), args.output.resolve())
            if any(output == path or output.is_relative_to(path)
                   for output in outputs for path in protected):
                raise ValueError("output must not overwrite target inputs or calibration")
            args.output.write_text(text, encoding="utf-8")
        else:
            print(text, end="")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(json.dumps(dict(ok=False, error=str(exc))))
        return 1


if __name__ == "__main__":
    sys.exit(main())

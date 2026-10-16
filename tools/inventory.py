#!/usr/bin/env python3
"""Reproducible ELF inventory, not a disassembler or a matching pipeline.

Python >= 3.10, standard library only. Run from the project root:
    python3 tools/inventory.py --output build/inventory.json

Public API: ELF(data: bytes, unit: str), build_inventory(game_dir, annotations_dir),
and main(argv=None). Malformed/unsupported ELF input raises ELFError (ValueError).
JSON v1 uses lowercase eight-digit hex VAs, integer file offsets/sizes, and
unit:VA function keys. Paths in the manifest are relative to their input root.
No timestamps, absolute paths, inferred function sizes, or CSV-derived symbols
are emitted. Zero-sized symbols hash the empty byte string. Conflicting sizes
or hashes have null respective aggregates; exact evidence remains in `symbols`.

JSON layout:
  units[unit]: path/size/sha256, header (entry/gp/gp_sources), sections,
    program_headers, symbol/function counts, excluded_functions, research_symbols.
  functions[unit:VA]: unit/va, name/names/aliases, size/sha256,
    file_unit/file_offset/section/section_index, symbols, annotation IDs.
    The representative symbol prefers the canonical unit, then ELF table order;
    file_unit and section_index always identify its actual executable and section.
  section_aliases: source/canonical unit and section indices plus equality proof.
  function_aliases[source_unit:VA]: canonical_unit:VA (ELF evidence, not CSV).
  annotation_manifest: input-relative path/unit/size/sha256 (null if missing).
  annotations: id/unit/path/line, hypothesis, status, function (key or null),
    va/name/description; invalid rows have raw instead of parsed fields.
  reports: tagged records (`kind`) referencing symbol/function/annotation IDs.
  totals: original/eligible/excluded FUNC symbols, canonical functions, alias and
    annotation counts, annotation_statuses, report_counts.
  schema_version: 1; policy: explicit identity, deduplication and annotation rules.

Unsupported ELF class/endianness/machine, ET_REL, compressed sections, and
SHN_XINDEX symbols fail explicitly. Extended ELF header section counts work.
NOBITS has no offsets/hashes and never supplies canonical function bytes.
GP is only taken from _gp or MIPS REGINFO evidence, not inferred for overlays.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct
import sys


INPUTS = (
    ("main", "SLUS_204.69", "slus_204.69_annotations.csv"),
    ("ov01", "OV01.OVL", "overlays/ov01_annotations.csv"),
    ("ov02", "OV02.OVL", "overlays/ov02_annotations.csv"),
    ("ov10", "OV10.OVL", "overlays/ov10_annotations.csv"),
    ("ov11", "OV11.OVL", "overlays/ov11_annotations.csv"),
    ("ov12", "OV12.OVL", "overlays/ov12_annotations.csv"),
)


def va(value):
    return f"0x{value:08x}"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class ELFError(ValueError):
    """Invalid ELF structure or an unsupported ELF encoding."""


class ELF:
    """Bounds-checked ELF32 LE MIPS reader; symbol values are virtual addresses."""

    def fail(self, message):
        raise ELFError(f"{self.unit}: {message}")

    def span(self, offset, size, context):
        if offset < 0 or size < 0 or offset > len(self.data) or size > len(self.data) - offset:
            self.fail(f"{context}: file range {offset}+{size} exceeds {len(self.data)} bytes")
        return self.data[offset:offset + size]

    def string(self, table, offset, context):
        if offset >= len(table):
            self.fail(f"{context}: string offset {offset} out of range")
        end = table.find(b"\0", offset)
        if end < 0:
            self.fail(f"{context}: unterminated string")
        return table[offset:end].decode("utf-8", errors="surrogateescape")

    def section_bytes(self, section):
        if section["type"] in (0, 8):
            return None
        return self.span(section["file_offset"], section["size"], "section data")

    def __init__(self, data, unit):
        self.data, self.unit = data, unit
        header = self.span(0, 52, "ELF header")
        if header[:7] != b"\x7fELF\x01\x01\x01":
            self.fail("expected ELF32 little-endian, ELF version 1")
        (elf_type, machine, version, entry, phoff, shoff, flags, ehsize,
         phentsize, phnum, shentsize, shnum, shstrndx) = struct.unpack_from("<HHIIIIIHHHHHH", header, 16)
        if machine != 8 or version != 1 or elf_type not in (2, 3):
            self.fail("expected MIPS ET_EXEC or ET_DYN, version 1 (not relocatable offsets)")
        if ehsize < 52:
            self.fail("ELF header size is less than 52")
        self.span(0, ehsize, "declared ELF header")
        if not shoff or shentsize < 40:
            self.fail("section table is missing or its entry size is less than 40")
        first = struct.unpack("<10I", self.span(shoff, 40, "section zero"))
        if first[1] != 0:
            self.fail("section zero is not SHT_NULL")
        if shnum == 0:
            shnum = first[5]
        if shstrndx == 0xffff:
            shstrndx = first[6]
        if phnum == 0xffff:
            phnum = first[7]
        if not shnum:
            self.fail("empty section table")
        self.span(shoff, shnum * shentsize, "section table")
        self.sections = []
        for index in range(shnum):
            fields = struct.unpack_from("<10I", data, shoff + index * shentsize)
            name, kind, sflags, address, offset, size, link, info, align, entsize = fields
            if sflags & 0x800:
                self.fail(f"section {index}: compressed sections are not supported")
            if kind not in (0, 8):
                self.span(offset, size, f"section {index}")
            if sflags & 2 and address + size > 0x100000000:
                self.fail(f"section {index}: virtual range overflows ELF32")
            self.sections.append(dict(index=index, name_offset=name, type=kind, flags=sflags,
                                      va=va(address), file_offset=None if kind in (0, 8) else offset,
                                      size=size, link=link, info=info, alignment=align, entry_size=entsize))
        if shstrndx >= shnum or (shstrndx and self.sections[shstrndx]["type"] != 3):
            self.fail("invalid section-name string table")
        names = self.section_bytes(self.sections[shstrndx]) if shstrndx else b"\0"
        for section in self.sections:
            section["name"] = self.string(names, section.pop("name_offset"), "section name")
            content = self.section_bytes(section)
            section["sha256"] = sha256(content) if content is not None else None

        self.program_headers = []
        gp_sources = []
        if phnum:
            if not phoff or phentsize < 32:
                self.fail("invalid program-header table")
            self.span(phoff, phnum * phentsize, "program-header table")
        for index in range(phnum):
            kind, offset, address, physical, filesz, memsz, pflags, align = struct.unpack_from(
                "<8I", data, phoff + index * phentsize)
            # An empty PT_LOAD file image needs no file bytes (ELF gABI 7.1/7.2).
            if kind != 1 or filesz:
                self.span(offset, filesz, f"program header {index}")
            if address + memsz > 0x100000000 or (kind == 1 and filesz > memsz):
                self.fail(f"program header {index}: invalid memory extent")
            if kind == 1 and align > 1 and (align & (align - 1) or address % align != offset % align):
                self.fail(f"program header {index}: invalid load alignment")
            self.program_headers.append(dict(index=index, type=kind, file_offset=offset,
                                             va=va(address), physical_address=va(physical),
                                             file_size=filesz, memory_size=memsz, flags=pflags, alignment=align))
            if kind == 0x70000000:  # PT_MIPS_REGINFO, final word is ri_gp_value.
                if filesz < 24:
                    self.fail("truncated PT_MIPS_REGINFO")
                gp_sources.append(dict(source=f"program_header:{index}", value=va(struct.unpack_from("<I", data, offset + 20)[0])))
        for section in self.sections:
            if section["type"] == 0x70000006:  # SHT_MIPS_REGINFO
                if section["size"] < 24:
                    self.fail("truncated SHT_MIPS_REGINFO")
                gp_sources.append(dict(source=f"section:{section['index']}", value=va(struct.unpack_from("<I", self.section_bytes(section), 20)[0])))

        self.symbols = []
        self.function_symbols = []
        self.research_symbols = []
        self.excluded_functions = []
        for table in self.sections:
            if table["type"] not in (2, 11):
                continue
            stride = table["entry_size"]
            if stride < 16 or table["size"] % stride:
                self.fail(f"symbol table {table['index']}: invalid entry size")
            link = table["link"]
            if link >= shnum or self.sections[link]["type"] != 3:
                self.fail(f"symbol table {table['index']}: invalid string-table link")
            strings = self.section_bytes(self.sections[link])
            count = table["size"] // stride
            if table["info"] > count:
                self.fail("symbol table local-symbol boundary out of range")
            for index in range(count):
                name, address, size, info, other, shndx = struct.unpack_from(
                    "<IIIBBH", data, table["file_offset"] + index * stride)
                name = self.string(strings, name, f"symbol {table['index']}:{index}")
                if shndx == 0xffff:
                    self.fail("SHN_XINDEX symbols are not supported")
                if 0 < shndx < 0xff00 and shndx >= shnum:
                    self.fail(f"symbol {name!r}: section index out of range")
                symbol = dict(id=f"{unit}:{table['index']}:{index}", unit=unit,
                              table_index=table["index"], symbol_index=index, name=name,
                              va=va(address), size=size, type=info & 15, binding=info >> 4,
                              other=other, section_index=shndx, section=None,
                              file_offset=None, sha256=None)
                self.symbols.append(symbol)
                if name == "_gp" and shndx != 0 and shndx != 0xfff2:
                    gp_sources.append(dict(source=symbol["id"], value=va(address)))
                if symbol["type"] not in (1, 2) and not name.startswith("gcc2_compiled"):
                    continue
                reason = None
                if shndx == 0:
                    reason = "undefined"
                elif shndx >= 0xff00:
                    reason = "special_section"
                else:
                    section = self.sections[shndx]
                    symbol["section"] = section["name"]
                    delta = address - int(section["va"], 16)
                    if delta < 0 or delta > section["size"] or size > section["size"] - delta:
                        self.fail(f"symbol {name!r}: virtual extent outside section {shndx}")
                    if section["type"] in (0, 8):
                        reason = "no_file_bytes"
                    elif not section["flags"] & 2:
                        reason = "non_allocated_section"
                    else:
                        # st_value is a VA, never a file offset.
                        offset = section["file_offset"] + delta
                        symbol["file_offset"] = offset
                        symbol["sha256"] = sha256(self.span(offset, size, f"symbol {name!r}"))
                if reason:
                    symbol["unavailable_reason"] = reason
                if symbol["type"] == 2:
                    (self.excluded_functions if reason else self.function_symbols).append(symbol)
                if symbol["type"] == 1 or name.startswith("gcc2_compiled"):
                    self.research_symbols.append(symbol)
        gp_values = sorted({item["value"] for item in gp_sources})
        self.header = dict(type=elf_type, machine=machine, flags=flags, entry=va(entry),
                           gp=gp_values[0] if len(gp_values) == 1 else None, gp_sources=gp_sources,
                           section_count=shnum, program_header_count=phnum)


def build_inventory(game_dir=Path("game/extracted-iso"), annotations_dir=Path("annotations")):
    """Read all six executables and reconcile annotations without trusting them."""
    game_dir, annotations_dir = Path(game_dir), Path(annotations_dir)
    elfs = {unit: ELF((game_dir / filename).read_bytes(), unit) for unit, filename, _ in INPUTS}
    reports = []
    section_aliases = []
    section_targets = {}
    main_elf, overlay = elfs["main"], elfs["ov02"]
    for section in main_elf.sections:
        if not section["flags"] & 2 or section["type"] in (0, 8) or not section["size"]:
            continue
        candidates = [s for s in overlay.sections if s["flags"] & 2 and s["type"] not in (0, 8)
                      and s["size"] and s["va"] == section["va"]]
        matches = [s for s in candidates if s["size"] == section["size"]
                   and main_elf.section_bytes(section) == overlay.section_bytes(s)]
        if len(matches) == 1:
            target = matches[0]
            section_targets[section["index"]] = target["index"]
            section_aliases.append(dict(source_unit="main", source_section=section["index"],
                                        canonical_unit="ov02", canonical_section=target["index"],
                                        va=section["va"], size=section["size"], sha256=section["sha256"],
                                        proof="equal_va_size_and_section_bytes"))
        elif candidates:
            reports.append(dict(kind="overlay_section_mismatch" if not matches else "ambiguous_section_alias",
                                unit="main", section_index=section["index"],
                                candidate_sections=[s["index"] for s in candidates]))

    functions = {}
    function_aliases = {}
    for unit, _, _ in INPUTS:
        for symbol in elfs[unit].function_symbols:
            target_unit = "ov02" if unit == "main" and symbol["section_index"] in section_targets else unit
            key = f"{target_unit}:{symbol['va']}"
            source_key = f"{unit}:{symbol['va']}"
            if key != source_key:
                function_aliases[source_key] = key
            function = functions.setdefault(key, dict(unit=target_unit, va=symbol["va"], symbols=[], annotations=[]))
            function["symbols"].append(symbol)
    for key, function in sorted(functions.items()):
        symbols = function["symbols"]
        symbols.sort(key=lambda s: (s["unit"] != function["unit"], s["unit"], s["table_index"], s["symbol_index"]))
        primary = symbols[0]
        names = sorted({s["name"] for s in symbols if s["name"]})
        function.update(name=primary["name"], names=names, aliases=[n for n in names if n != primary["name"]],
                        section=primary["section"], section_index=primary["section_index"],
                        file_unit=primary["unit"], file_offset=primary["file_offset"])
        sizes, hashes = {s["size"] for s in symbols}, {s["sha256"] for s in symbols}
        function["size"] = next(iter(sizes)) if len(sizes) == 1 else None
        function["sha256"] = next(iter(hashes)) if len(hashes) == 1 else None
        if len(sizes) > 1 or len(hashes) > 1:
            reports.append(dict(kind="function_extent_conflict", function=key,
                                symbols=[s["id"] for s in symbols]))
        grouped = defaultdict(list)
        for symbol in symbols:
            grouped[symbol["unit"]].append(symbol["id"])
        for unit, ids in sorted(grouped.items()):
            if len(ids) > 1:
                reports.append(dict(kind="duplicate_function_address", unit=unit, function=key, symbols=ids))

    overlay_ranges = [(unit, int(s["va"], 16), int(s["va"], 16) + s["size"])
                      for unit, elf in elfs.items() if unit != "main"
                      for s in elf.sections if s["flags"] & 2 and s["size"]]
    by_name = defaultdict(set)
    by_address = defaultdict(list)
    for key, function in sorted(functions.items()):
        by_address[function["va"]].append(key)
        for name in function["names"]:
            by_name[function["unit"], name].add(key)
    annotations = []
    annotation_manifest = []
    duplicates_by_va, duplicates_by_name = defaultdict(list), defaultdict(list)
    for unit, _, filename in INPUTS:
        path = annotations_dir / filename
        if not path.exists():
            annotation_manifest.append(dict(unit=unit, path=filename, sha256=None, size=None))
            reports.append(dict(kind="missing_annotation_file", unit=unit, path=filename))
            continue
        data = path.read_bytes()
        annotation_manifest.append(dict(unit=unit, path=filename, sha256=sha256(data), size=len(data)))
        for line_number, raw in enumerate(data.decode("utf-8-sig").splitlines(), 1):
            if not raw.strip():
                continue
            item = dict(id=f"{unit}:{line_number}", unit=unit, path=filename, line=line_number,
                        hypothesis=True, function=None)
            annotations.append(item)
            address_text, comma, text = raw.partition(",")
            name, separator, description = text.partition(" - ")
            try:
                address_text = address_text.strip()
                if not comma or not name.strip() or not address_text.lower().startswith("0x"):
                    raise ValueError("expected 0xVA, name[ - description]")
                address = int(address_text, 16)
                if not 0 <= address <= 0xffffffff:
                    raise ValueError("address outside ELF32")
            except ValueError as error:
                item.update(status="invalid", raw=raw)
                reports.append(dict(kind="invalid_annotation", annotation=item["id"], detail=str(error)))
                continue
            item.update(va=va(address), name=name.strip(), description=description.strip() if separator else "")
            duplicates_by_va[unit, item["va"]].append(item["id"])
            duplicates_by_name[unit, item["name"]].append(item["id"])
            key = f"{unit}:{item['va']}"
            function = functions.get(key)
            candidates = sorted(by_name[unit, item["name"]] - {key})
            if unit == "main" and any(start <= address < end for _, start, end in overlay_ranges):
                item["status"] = "quarantined"
                reports.append(dict(kind="main_annotation_in_overlay", annotation=item["id"],
                                    overlay_units=sorted({u for u, start, end in overlay_ranges if start <= address < end}),
                                    candidate_functions=[k for k in by_address[item["va"]] if not k.startswith("main:")]))
                continue
            if function:
                item["function"] = key
                function["annotations"].append(item["id"])
                item["status"] = "matched" if item["name"] in function["names"] else "name_mismatch"
                if item["status"] == "name_mismatch":
                    reports.append(dict(kind="annotation_name_mismatch", annotation=item["id"],
                                        function=key, elf_names=function["names"]))
            else:
                item["status"] = "unmatched"
                reports.append(dict(kind="annotation_address_unmatched", annotation=item["id"], va=item["va"], unit=unit))
            if candidates and (not function or item["name"] not in function["names"]):
                reports.append(dict(kind="annotation_name_at_other_address", annotation=item["id"],
                                    candidate_functions=candidates))
    for kind, groups in (("duplicate_annotation_address", duplicates_by_va), ("duplicate_annotation_name", duplicates_by_name)):
        for (unit, value), ids in sorted(groups.items()):
            if len(ids) > 1:
                reports.append(dict(kind=kind, unit=unit, value=value, annotations=ids))

    units = {}
    for unit, filename, _ in INPUTS:
        elf = elfs[unit]
        if not any(s["type"] in (2, 11) for s in elf.sections):
            reports.append(dict(kind="missing_symbol_table", unit=unit))
        if len({s["value"] for s in elf.header["gp_sources"]}) > 1:
            reports.append(dict(kind="gp_conflict", unit=unit, sources=elf.header["gp_sources"]))
        units[unit] = dict(path=filename, size=len(elf.data), sha256=sha256(elf.data), header=elf.header,
                           sections=elf.sections, program_headers=elf.program_headers,
                           symbol_count=len(elf.symbols),
                           original_func_count=sum(s["type"] == 2 for s in elf.symbols),
                           eligible_func_count=len(elf.function_symbols),
                           canonical_func_count=sum(f["unit"] == unit for f in functions.values()),
                           excluded_functions=elf.excluded_functions, research_symbols=elf.research_symbols)
    reports.sort(key=lambda r: json.dumps(r, sort_keys=True))
    return dict(schema_version=1,
                policy=dict(function_identity="unit:VA", annotation_authority="hypothesis_only",
                            deduplication="main_to_ov02_only_equal_va_size_and_section_bytes",
                            main_overlay_annotations="quarantine_without_import",
                            sizes="ELF_st_size_no_inference", addresses="hex_VA_offsets_are_integers"),
                units=units, section_aliases=section_aliases, function_aliases=function_aliases,
                functions=dict(sorted(functions.items())), annotation_manifest=annotation_manifest,
                annotations=annotations, reports=reports,
                totals=dict(original_func_symbols=sum(u["original_func_count"] for u in units.values()),
                            eligible_func_symbols=sum(u["eligible_func_count"] for u in units.values()),
                            canonical_functions=len(functions), function_aliases=len(function_aliases),
                            excluded_func_symbols=sum(len(u["excluded_functions"]) for u in units.values()),
                            annotations=len(annotations), annotation_statuses=dict(sorted(Counter(a["status"] for a in annotations).items())),
                            report_counts=dict(sorted(Counter(r["kind"] for r in reports).items()))))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, required=True, help="derived JSON destination")
    parser.add_argument("--game-dir", type=Path, default=Path("game/extracted-iso"))
    parser.add_argument("--annotations-dir", type=Path, default=Path("annotations"))
    args = parser.parse_args(argv)
    try:
        inventory = build_inventory(args.game_dir, args.annotations_dir)
        payload = json.dumps(inventory, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8", newline="\n")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(inventory["totals"], sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())

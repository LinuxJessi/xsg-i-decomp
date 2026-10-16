#!/usr/bin/env python3
"""Bounded local C hypotheses, not original-compiler selection or game recovery.

Uses the baseline inventory/target collector. Output must be a new directory
below project build/ or .work/attempts/. No SDK headers, uploads, or installation.
Assembly is retained unchanged. Only a full linked text/VA/size equality may
receive exact_text_equal; symbol-only equality never hides section padding.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
for parent in Path(__file__).resolve().parents:
    if (parent / "tools/inventory.py").is_file():
        sys.path.insert(0, str(parent))
        break
from tools.inventory import ELF, sha256
from tools.toolchain_probe import collect, instruction_features, TARGET_MANIFEST_SHA256

REVISION = "slus-20469-412d448de315"
CALIBRATION_SHA256 = "3d74f49fc5cc4f1011a20cbcd656eff68201ee9984a9ca1cf5b9f6b216f4c48d"
COMMON_FLAGS = ["-nostdinc", "-fno-builtin"]
MATRIX = [["-O" + str(o), "-G" + str(g)] for o in range(4) for g in (0, 8)]
# These are deliberately hypotheses, not recovered declarations or shared types.
CASES = [
    ("bcd", "main", "0x0021d2c8", "BCD2INT",
     "int BCD2INT(unsigned char x) { return (x >> 4) * 10 + (x & 15); }", {}),
    ("hex", "main", "0x0021ad08", "hex2val",
     "unsigned char hex2val(signed char x) { if (x >= '0' && x <= '9') return x-'0'; if (x >= 'a' && x <= 'f') return x-'a'+10; if (x >= 'A' && x <= 'F') return x-'A'+10; return 0; }", {}),
    ("bonus", "ov11", "0x00a009a8", "RES_GetBonusTime",
     "extern int *res; int RES_GetBonusTime(void) { return res[0]; }", {"res": "0x00a0dfa0"}),
    ("speed", "ov11", "0x00a009b8", "RES_GetRealSpeed",
     "extern int *res; int RES_GetRealSpeed(void) { return res[1]; }", {"res": "0x00a0dfa0"}),
    ("shop", "ov11", "0x00a00a08", "RES_ShopDataName",
     "struct Shop { char pad[12]; char name[112]; }; extern char *res; char *RES_ShopDataName(int i) { return ((struct Shop *)res)[i].name; }", {"res": "0x00a0dfa0"}),
    ("field", "main", "0x00243c10", "getScriptFlag",
     "struct Script { char pad[292]; int flags; }; int getScriptFlag(struct Script *p) { return p->flags; }", {}),
    ("gp", "main", "0x00261290", "SCRIPT_getCfTime",
     "extern int cf_time; int SCRIPT_getCfTime(void) { return cf_time; }", {"cf_time": "0x004da44c", "_gp": "0x004dfb70"}),
    ("font", "main", "0x0021b590", "xglFontGetFlags",
     "extern unsigned short font_flags; unsigned short xglFontGetFlags(void) { return font_flags; }", {"font_flags": "0x00881188"}),
]
ABI_SOURCE = """struct Layout { char c; void *p; long l; double d; float f; long long ll; };
#define OFF(member) ((unsigned int)&((struct Layout *)0)->member)
unsigned int abi_model[] = { sizeof(char), sizeof(short), sizeof(int), sizeof(long),
 sizeof(long long), sizeof(void *), sizeof(float), sizeof(double),
 sizeof(struct Layout), __alignof__(struct Layout), OFF(p), OFF(l), OFF(d), OFF(f), OFF(ll),
 __alignof__(double), __alignof__(long long) };
extern int sink(int, int, int, int, int, int, int, int, int);
int abi_call(int x) { return sink(x,2,3,4,5,6,7,8,9)+x; }
float abi_float(float a, float b) { return a+b; }
double abi_double(double a, double b) { return a+b; }
long long abi_integer(long long a, long long b) { return a+b; }
"""
ABI_KEYS = ["char", "short", "int", "long", "long_long", "pointer", "float", "double",
            "struct_size", "struct_align", "offset_p", "offset_l", "offset_d", "offset_f",
            "offset_ll", "align_double", "align_long_long"]


def digest(path):
    return sha256(Path(path).read_bytes())


def validate_lock(lock):
    if (lock["schema_version"] != 1 or lock["status"] != "candidate_not_selected_original"
            or lock["revision"] != REVISION or lock["target_manifest_sha256"] != TARGET_MANIFEST_SHA256
            or lock["calibration_sha256"] != CALIBRATION_SHA256
            or lock["common_flags"] != COMMON_FLAGS or lock["matrix"] != MATRIX):
        raise ValueError("candidate lock identity/options mismatch")
    tools = lock["tools"]
    if set(tools) != {"compiler", "cc1", "cpp", "specs", "assembler", "linker", "objdump"}:
        raise ValueError("incomplete tool pins")
    if set(lock["host_support"]) != {"libc", "loader", "libzstd"}:
        raise ValueError("incomplete dynamic host support pins")
    for name, pin in {**tools, **lock["host_support"]}.items():
        path = Path(pin["path"])
        if not path.is_absolute() or digest(path) != pin["sha256"]:
            raise ValueError("payload hash mismatch: " + name)
    support_dir = Path(tools["compiler"]["path"]).parent
    if {p.name for p in support_dir.iterdir()} != {"ee-gcc", "cc1", "cpp", "specs"}:
        raise ValueError("unexpected compiler support directory contents")
    if any(Path(tools[k]["path"]).parent != support_dir for k in ("cc1", "cpp", "specs")):
        raise ValueError("compiler support paths must share the pinned directory")
    return {key: pin["path"] for key, pin in tools.items()}


def prepare_output(root, output, inputs):
    lexical, resolved = output.absolute(), output.resolve()
    allowed = [root / "build", root / ".work/attempts"]
    protected = [root / "game", root / "game/extracted-iso", root / "config", *inputs]
    protected += [p.resolve() for p in protected]
    if any(p == q or p.is_relative_to(q) for p in (lexical, resolved) for q in protected):
        raise ValueError("output aliases a protected input")
    if not any(lexical.is_relative_to(p) and resolved.is_relative_to(p.resolve()) for p in allowed):
        raise ValueError("output must be below local build/ or .work/attempts/")
    if not resolved.parent.is_dir():
        raise ValueError("output parent must already exist")
    resolved.mkdir(exist_ok=False)
    return resolved


def compile_command(tools, flags, source, assembly):
    if flags not in MATRIX:
        raise ValueError("flags outside bounded matrix")
    return [tools["compiler"], "-B" + str(Path(tools["compiler"]).parent) + "/",
            *COMMON_FLAGS, *flags, "-S", str(source), "-o", str(assembly)]


def compare_linked(elf, entry, name, reference):
    """No instruction normalization, padding removal, or relocatable comparison."""
    if sha256(reference) != entry["sha256"] or len(reference) != entry["size"]:
        raise ValueError("reference extent hash/size mismatch")
    if elf.header["type"] != 2 or any(s["type"] in (4, 9) and s["size"] for s in elf.sections):
        raise ValueError("comparison requires fully linked ET_EXEC without relocations")
    symbols = [s for s in elf.function_symbols if s["name"] == name]
    text = [s for s in elf.sections if s["name"] == ".text"]
    if len(symbols) != 1 or len(text) != 1:
        raise ValueError("ambiguous function/text extent")
    symbol, section = symbols[0], text[0]
    if symbol["va"] != entry["va"] or section["va"] != entry["va"]:
        raise ValueError("linked VA differs from unit-specific target VA")
    emitted = elf.section_bytes(section)
    body = elf.span(symbol["file_offset"], symbol["size"], "candidate FUNC")
    extra_code = any(s["flags"] & 4 and s["size"] and s["index"] != section["index"] for s in elf.sections)
    return dict(exact_text_equal=emitted == reference and body == reference and not extra_code,
                function_extent_equal=body == reference, extra_executable_sections=extra_code,
                text_size=len(emitted), function_size=len(body), text_sha256=sha256(emitted),
                function_sha256=sha256(body), reference_size=len(reference),
                features=instruction_features(body, int(entry["va"], 16)))


def calibrate(root, lock_path, output, asm_only=False):
    root = root.resolve()
    lock = json.loads(lock_path.read_bytes())
    tools = validate_lock(lock)  # Pin every executable and support payload before any execution.
    calibration = root / "config/calibration.json"
    if digest(calibration) != CALIBRATION_SHA256:
        raise ValueError("calibration differs from the fixed 18-reference corpus")
    target = collect(root, calibration)
    if target["revision"] != REVISION or target["calibration_functions_verified"] != 18:
        raise ValueError("fixed B00 target/corpus mismatch")
    entries = {(x["unit"], x["va"]): x for x in json.loads(calibration.read_bytes())["functions"]}
    output = prepare_output(root, output, [lock_path, *[Path(x["path"]) for x in lock["tools"].values()]])
    env = {"LC_ALL": "C", "PATH": "/usr/bin:/bin", "HOME": str(output), "TMPDIR": str(output)}
    result = dict(schema_version=1, revision=REVISION, lock_sha256=digest(lock_path), runner_sha256=digest(Path(__file__)),
                  target_manifest_sha256=TARGET_MANIFEST_SHA256, calibration_sha256=CALIBRATION_SHA256,
                  compiler_selected=False, original_abi_proven=False, matched_c_functions=0,
                  mode="asm_guidance_only" if asm_only else "linked_candidate_calibration",
                  environment=env, cases=[], commands=[], abi=[])

    def run(argv, directory, label):
        # Recheck pins before each child process; this is not a concurrent-write security boundary.
        validate_lock(lock)
        raw = json.dumps(argv, separators=(",", ":")).encode()
        log = directory / (label + ".log")
        with log.open("xb") as stream:
            proc = subprocess.Popen(argv, cwd=directory, env=env, stdout=stream,
                                    stderr=subprocess.STDOUT, start_new_session=True)
            try:
                rc = proc.wait(timeout=20)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
                rc = 124
        result["commands"].append(dict(argv=argv, argv_sha256=sha256(raw), cwd=str(directory),
                                       returncode=rc, log=str(log), log_sha256=digest(log)))
        return rc

    def build(source, flags, directory, address, symbols):
        # GCC embeds its source argument in .file; cwd already selects this case.
        if run(compile_command(tools, flags, source.name, "candidate.s"), directory, "compile"):
            return None, "compile_failed"
        if asm_only:
            return None, "assembly_only_not_exact"
        if run([tools["assembler"], "-EL", "-march=r5900", "-mabi=eabi", flags[1],
                "-o", "candidate.o", "candidate.s"], directory, "assemble"):
            return None, "assemble_failed"
        script = "ENTRY(" + ("abi_call" if source.name == "abi.c" else "_candidate_entry") + ")\nSECTIONS {\n"
        script += ".text " + address + " : { *(.text .text.*) }\n"
        script += "\n".join(k + " = " + v + ";" for k, v in symbols.items()) + "\n}\n"
        (directory / "layout.ld").write_text(script, encoding="ascii")
        if run([tools["linker"], "-m", "elf32lr5900", "-T", "layout.ld", "-o", "candidate.elf", "candidate.o"], directory, "link"):
            return None, "link_failed"
        if run([tools["objdump"], "-dr", "-M", "no-aliases", "candidate.elf"], directory, "disassembly"):
            return None, "disassembly_failed"
        return ELF((directory / "candidate.elf").read_bytes(), "candidate"), "linked"

    run([tools["compiler"], "-B" + str(Path(tools["compiler"]).parent) + "/", "-v"], output, "compiler-version")
    for flags in MATRIX:
        label = "_".join(x[1:] for x in flags)
        for case, unit, va, name, code, symbols in CASES:
            directory = output / (case + "_" + label)
            directory.mkdir()
            source = directory / "candidate.c"
            source.write_text(code + "\n", encoding="ascii")
            elf, status = build(source, flags, directory, va, dict(symbols, _candidate_entry=va))
            entry = entries[(unit, va)]
            record = dict(case=case, revision=REVISION, unit=unit, va=va, original_name=name,
                          binary_sha256=entry["binary_sha256"], flags=flags, status=status,
                          source_sha256=digest(source), directory=str(directory), exact_text_equal=False)
            if elf:
                binary = root / "game/extracted-iso" / target["units"][unit]["path"]
                data = binary.read_bytes()
                if sha256(data) != entry["binary_sha256"]:
                    raise ValueError("target changed during compilation")
                reference = data[entry["file_offset"]:entry["file_offset"] + entry["size"]]
                record.update(compare_linked(elf, entry, name, reference))
            result["cases"].append(record)
        directory = output / ("abi_" + label)
        directory.mkdir()
        source = directory / "abi.c"
        source.write_text(ABI_SOURCE, encoding="ascii")
        # Synthetic import addresses measure code generation, not runtime correctness.
        elf, status = build(source, flags, directory, "0x00200000", {"sink": "0x00220000", "dpadd": "0x00220100"})
        abi = dict(flags=flags, status=status, source_sha256=digest(source), directory=str(directory),
                   evidence_level="candidate_only_not_original_ABI")
        if elf:
            models = [s for s in elf.symbols if s["name"] == "abi_model" and s["file_offset"] is not None]
            if len(models) != 1 or models[0]["size"] != 4 * len(ABI_KEYS):
                raise ValueError("unexpected candidate ABI probe layout")
            values = struct.unpack("<" + "I" * len(ABI_KEYS), elf.span(models[0]["file_offset"], models[0]["size"], "ABI model"))
            abi["measured"] = dict(zip(ABI_KEYS, values))
            abi["functions"] = [{"name": s["name"], "size": s["size"], "features": instruction_features(
                elf.span(s["file_offset"], s["size"], "ABI function"), int(s["va"], 16))} for s in elf.function_symbols]
        result["abi"].append(abi)
    result["exact_text_equal_cases"] = sum(c["exact_text_equal"] for c in result["cases"])
    result["failed_cases"] = sum(c["status"].endswith("failed") for c in result["cases"] + result["abi"])
    result["artifacts"] = {str(p.relative_to(output)): digest(p) for p in sorted(output.rglob("*")) if p.is_file()}
    (output / "results.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--lock", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--asm-only", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = calibrate(args.root, args.lock.resolve(), args.output_dir, args.asm_only)
        print(json.dumps({k: result[k] for k in ("mode", "exact_text_equal_cases", "failed_cases", "compiler_selected")}))
        return 1 if result["failed_cases"] else 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(json.dumps(dict(ok=False, error=str(exc))))
        return 1


if __name__ == "__main__":
    sys.exit(main())

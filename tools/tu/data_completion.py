"""Replace evidenced, unreferenced DATA alignment fragments with linker fill.

No storage definition or recovered-byte counter is manufactured here. A gap
must follow compiler-owned storage, end at the next compiler piece or TU end,
contain no original symbol/reference, and be exactly one bounded ALIGN step.
The ordinary map checker and complete original-file gate remain mandatory.
"""
from pathlib import Path
import re
import sys


def mark_alignment(root, unit_dir, manifest):
    # Configure must work in a clean checkout, before any C object exists.
    # All decisions below depend on originals, source and registered ownership.
    sys.path.insert(0, str(Path(root) / 'tools'))
    from tu.objdiff_inputs import parsed_elf, original_references
    from tu.verify_report import read

    filename = read(Path(root) / 'config/originals.json')['units']['main']['file']
    original = parsed_elf((Path(unit_dir) / 'orig' / filename).read_bytes())
    proofs = []
    for tu in manifest['tus']:
        if tu.get('mode') != 'c' or not tu.get('in_scope'):
            continue
        source = Path(root) / (tu.get('source', {}).get('path') or tu['path'])
        if not source.is_file():
            continue
        text = source.read_text()
        text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
        # Reviewed standalone assembly does not change a C DATA gap's
        # alignment proof. Unrecovered function scaffolding is still excluded.
        if re.search(r'\bINCLUDE_ASM\s*\(', text):
            continue
        for section_name, section in tu['sections'].items():
            pieces = section.get('pieces', [])
            original_section = original.section(section_name)
            for index, piece in enumerate(pieces):
                if piece.get('c_split') or index == 0:
                    continue
                lo, hi = (int(piece[k], 16) for k in ('start', 'end'))
                first = index
                while first and not pieces[first - 1].get('c_split'):
                    first -= 1
                last = index
                while last + 1 < len(pieces) and not pieces[last + 1].get('c_split'):
                    last += 1
                if not first:
                    continue
                group_lo = int(pieces[first]['start'], 16)
                group_hi = int(pieces[last]['end'], 16)
                if int(pieces[first - 1]['end'], 16) != group_lo:
                    continue
                if any(pieces[j]['end'] != pieces[j + 1]['start']
                       for j in range(first, last)):
                    continue
                if last + 1 < len(pieces):
                    following = pieces[last + 1]
                    if int(following['start'], 16) != group_hi:
                        continue
                elif group_hi != int(section['end'], 16):
                    continue
                # Consider only bounded DATA alignments, up to 128 bytes.
                # A larger zero allocation is storage, not inferred alignment.
                if not any((group_lo + n - 1) // n * n == group_hi
                           for n in (2, 4, 8, 16, 32, 64, 128)):
                    continue
                alignment = next((n for n in (2, 4, 8, 16, 32, 64, 128)
                                  if lo < hi and (lo + n - 1) // n * n == hi), None)
                if alignment is None:
                    continue
                if not original_section.addr <= lo < hi <= original_section.addr + original_section.size:
                    continue
                if original_section.type != 8:
                    a = original_section.offset + lo - original_section.addr
                    if any(original.data[a:a + hi - lo]):
                        continue
                # Even unspecified original labels are real identities; never
                # discard one as padding merely because its storage is zero.
                if any(s.shndx == original_section.index and s.type != 3 and s.name
                       and (lo <= s.value < hi or
                            s.size and s.value < hi and lo < s.value + s.size)
                       for s in original.symbols):
                    continue
                if original_references(original, lo, hi):
                    continue
                proof = dict(kind='unreferenced_original_alignment',
                             range=[piece['start'], piece['end']], alignment=alignment,
                             original_symbols_absent=True, original_references_absent=True,
                             zero_or_nobits=True)
                piece['linker_padding'] = proof
                for row in manifest['sections'][section_name]['order']:
                    if row['tu'] == tu['name'] and row['piece'] == piece['name']:
                        row['linker_padding'] = proof
                proofs.append(dict(tu=tu['id'], section=section_name, **proof))
    return proofs

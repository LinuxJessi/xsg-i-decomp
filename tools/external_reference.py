"""Pinned external address witnesses, never generated data or recovered pointees.

Legacy references do not enter this opt-in contract. File-backed symbols retain
complete byte witnesses; anonymous references prove only an allocated NOBITS
range in one identified original ELF, not a source object or runtime contents.
"""
import hashlib


def require(ok, message):
    if not ok:
        raise ValueError(message)


SECTION_FIELDS = ('index', 'name', 'type', 'flags', 'va', 'size')
SYMBOL_FIELDS = ('table_index', 'symbol_index', 'name', 'va', 'size',
                 'type', 'binding', 'other', 'section_index')


def validate(name, ref, elf, section, data, binary):
    require(set(ref) == {'unit', 'va', 'size', 'sha256', 'symbols', 'witness'},
            'qualified reference fields differ from contract')
    witness = ref['witness']
    require(isinstance(witness, dict) and witness.get('kind') in ('symbol', 'anonymous_nobits'),
            'unsupported reference witness kind')
    kind = witness['kind']
    require(set(witness) == {'kind', 'binary_sha256', 'section',
                             'symbol' if kind == 'symbol' else 'range'},
            'reference witness fields differ from contract')
    require(elf.unit == ref['unit'] and witness['binary_sha256'] == binary['sha256']
            == hashlib.sha256(elf.data).hexdigest(), 'reference binary/unit witness mismatch')
    require(witness['section'] == {k: section[k] for k in SECTION_FIELDS},
            'reference section boundary witness mismatch')
    start, size = int(ref['va'], 16), ref['size']
    require(type(size) is int and 0 < size <= 1048576
            and int(section['va'], 16) <= start
            and start + size <= int(section['va'], 16) + section['size']
            and section['flags'] & 2, 'reference range outside allocated section')
    require(ref['sha256'] == (hashlib.sha256(data).hexdigest() if data is not None else None),
            'external byte witness mismatch')
    require(name != '_gp', 'qualified reference cannot replace original GP contract')
    if kind == 'symbol':
        symbol = witness['symbol']
        require(isinstance(symbol, dict) and set(symbol) == set(SYMBOL_FIELDS),
                'invalid exact reference symbol record')
        matches = [s for s in elf.symbols if all(s[k] == symbol[k] for k in SYMBOL_FIELDS)]
        require(len(matches) == 1 and symbol['name'] == name and symbol['va'] == ref['va']
                and symbol['size'] == size and symbol['section_index'] == section['index'],
                'exact reference symbol witness mismatch')
        # EE overlays can combine objects and functions in one executable section.
        require(symbol['type'] in (1, 2) and section['type'] in (1, 8)
                and (symbol['type'] != 2 or section['type'] == 1 and section['flags'] & 4 and data is not None),
                'reference symbol/section kind mismatch')
    else:
        require(witness['range'] == dict(va=ref['va'], size=size,
                    qualification='allocated-nobits-address-range-only'),
                'anonymous NOBITS range is not explicitly qualified')
        require(section['type'] == 8 and section['file_offset'] is None
                and not section['flags'] & 4 and data is None and ref['symbols'] == [],
                'anonymous reference requires non-executable NOBITS without symbols/bytes')
        # Do not disguise a named object (including an interior address) as anonymous.
        named = [s for s in elf.symbols if s['name'] and s['type'] != 3
                 and s['section_index'] == section['index']
                 and int(s['va'], 16) < start + size
                 and start < int(s['va'], 16) + max(s['size'], 1)]
        require(not named, 'anonymous NOBITS range overlaps an original named symbol')

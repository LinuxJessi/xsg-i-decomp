"""Compiler storage parser shared by public map checks and data auditing."""
import re


def emitted_storage(text):
    """Compiler-authored storage directives outside inline ASM blocks.

    COMMON/SCOMMON entries are not NOBITS bytes. Their compiler declaration,
    linked address, size, binding, and references are checked separately.
    """
    app = False
    section = '.text'
    previous_section = section
    out = {}
    active_label = None
    for line in text.splitlines():
        if line.strip() == '#APP':
            app = True
            continue
        if line.strip() == '#NO_APP':
            app = False
            continue
        if app:
            continue
        sec = re.match(r'\s*\.section\s+([^,\s]+)', line)
        simple = re.match(r'\s*(\.(?:text|init|fini|data|sdata|bss|sbss|scommon|rodata|rdata|lit4|lit8))\s*(?:$|#)', line)
        if sec:
            previous_section = section
            section = sec[1]
            active_label = None
        elif simple:
            previous_section = section
            section = simple[1]
            active_label = None
        elif re.match(r'\s*\.previous\s*(?:$|#)', line):
            section, previous_section = previous_section, section
            active_label = None
        common = re.match(r'\s*\.(comm|lcomm)\s+([^,\s]+)\s*,\s*(0x[\da-fA-F]+|\d+)(?:\s*,\s*(0x[\da-fA-F]+|\d+))?', line)
        if common:
            name, size = common[2], int(common[3], 0)
            align = int(common[4], 0) if common[4] else 1
            out[name] = dict(name=name, section='COMMON' if common[1] == 'comm' else '.bss',
                             size=size, alignment=align, directive=common[1], extent_kind='common-directive')
            active_label = None
            continue
        label = re.match(r'^\s*([A-Za-z_.$][\w.$]*):\s*(?:$|#)', line)
        if label and section in ('.bss', '.sbss', '.scommon'):
            active_label = label[1]
            out[active_label] = dict(name=active_label, section=section, size=0,
                                     alignment=1, extent_kind='section-label')
            continue
        align = re.match(r'\s*\.align\s+(0x[\da-fA-F]+|\d+)', line)
        if align and active_label in out:
            exponent = int(align[1], 0)
            if exponent < 32:
                out[active_label]['alignment'] = max(out[active_label].get('alignment', 1), 1 << exponent)
            continue
        balign = re.match(r'\s*\.balign\s+(0x[\da-fA-F]+|\d+)', line)
        if balign and active_label in out:
            out[active_label]['alignment'] = max(out[active_label].get('alignment', 1), int(balign[1], 0))
            continue
        space = re.match(r'\s*\.(?:space|skip|zero)\s+(0x[\da-fA-F]+|\d+)', line)
        if space and active_label in out:
            out[active_label]['size'] += int(space[1], 0)
            out[active_label]['extent_kind'] = 'label-space'
    return out

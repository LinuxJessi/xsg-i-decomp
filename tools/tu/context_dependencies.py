"""Diagnose same-TU context effects without granting recovery credit.

The target definition is fixed. Other original definitions, declarations and
inline helpers may differ, in original function order. An observation concerns
the changed context as a group; it does not prove each provider indispensable.
"""
from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path


SCHEMA = 'tu-context-probe/1'
NOTICE = ('DIAGNOSTIC ONLY: a context observation is not an exact candidate, '
          'an accepted dependency, or permission to change the compiler contract.')


class ContextError(ValueError):
    pass


def digest(text):
    return hashlib.sha256(text.encode('utf-8', errors='surrogateescape')).hexdigest()


def tokens(text, editor):
    return tuple(editor.c_tokens(text))


def source_context(base_text, context_text, function, functions, editor, unit_dir=None):
    """Require one complete, ordered original TU and an unchanged C target."""
    try:
        base = editor.TU(base_text, functions=functions, unit_dir=unit_dir)
        context = editor.TU(context_text, functions=functions, unit_dir=unit_dir)
        before, after = base.block(function), context.block(function)
        if before.state != 'c' or after.state != 'c' or before.conditional or after.conditional:
            raise ContextError('the target must be an unconditional C definition in both sources')
        target_tokens = tokens(base.block_text(before), editor)
        if target_tokens != tokens(context.block_text(after), editor):
            raise ContextError('context probe refuses a changed target definition')
        changes = []
        for name in functions:
            if name == function:
                continue
            old, new = base.block(name), context.block(name)
            old_text, new_text = base.block_text(old), context.block_text(new)
            if tokens(old_text, editor) != tokens(new_text, editor):
                changes.append(dict(function=name, before=old.state, after=new.state,
                                    before_sha256=digest(old_text), after_sha256=digest(new_text)))
        free_before = [value for item in base.free_items()
                       if (value := tokens(item.text(base_text), editor))]
        free_after = [value for item in context.free_items()
                      if (value := tokens(item.text(context_text), editor))]
        helpers_before = [(b.name, tokens(base.block_text(b), editor))
                          for b in base.blocks if b.role == 'helper']
        helpers_after = [(b.name, tokens(context.block_text(b), editor))
                         for b in context.blocks if b.role == 'helper']
    except (editor.ParseError, editor.EditError, editor.LexError) as exc:
        raise ContextError(f'cannot establish the original TU context: {exc}') from exc
    kinds = []
    if changes:
        kinds.append('function_definitions')
    if free_before != free_after:
        kinds.append('declarations_or_data')
    if helpers_before != helpers_after:
        kinds.append('inline_or_unmapped_helpers')
    if not kinds:
        raise ContextError('the two sources have no compilation-context change')
    return dict(target_definition_unchanged=True, target_source_name=before.name,
                target_tokens_sha256=digest(json.dumps(target_tokens)),
                base_source_sha256=digest(base_text), context_source_sha256=digest(context_text),
                changed_functions=changes, context_kinds=kinds)


def emitted(record):
    return dict(bytes_sha256=hashlib.sha256(record['bytes']).hexdigest(),
                size=len(record['bytes']), offset=record['offset'], section=record['section'],
                relocations=[list(row) for row in record['relocations']])


def relocation_fields_only(before, after):
    """Recognize changed relocation fields; never mask bytes for acceptance."""
    left, right = before['bytes'], after['bytes']
    if len(left) != len(right) or len(left) % 4 or left == right:
        return False
    if before['relocations'] != after['relocations']:
        return False
    masks = {}
    for offset, kind, *_rest in before['relocations']:
        if kind == 4:                    # R_MIPS_26
            mask = 0x03ffffff
        elif kind in (5, 6, 7):           # HI16, LO16, GPREL16
            mask = 0x0000ffff
        elif kind == 2:                  # R_MIPS_32
            mask = 0xffffffff
        else:
            continue
        masks[offset] = masks.get(offset, 0) | mask
    for offset in range(0, len(left), 4):
        difference = struct.unpack_from('<I', left, offset)[0] ^ struct.unpack_from('<I', right, offset)[0]
        if difference & ~masks.get(offset, 0):
            return False
    return True


def observation(source, before, after):
    """Describe the observed effect separately from the suspected cause."""
    result = dict(schema=SCHEMA, **source, note=NOTICE, acceptance=False,
                  individual_dependencies_proven=False)
    if not before.get('ok') or not after.get('ok'):
        return dict(result, effect='inconclusive', context_sensitive=None,
                    error='both variants must compile and expose the target symbol')
    left, right = before['candidate'], after['candidate']
    changed_bytes = left['bytes'] != right['bytes']
    changed_relocations = left['relocations'] != right['relocations']
    if changed_bytes:
        effect = 'relocation_fields' if relocation_fields_only(left, right) else 'code_generation'
    elif changed_relocations:
        effect = 'symbol_binding'
    elif (left['offset'], left['section']) != (right['offset'], right['section']):
        effect = 'placement_only'
    else:
        effect = 'no_observed_effect'
    return dict(result, effect=effect, context_sensitive=effect != 'no_observed_effect',
                before=emitted(left), after=emitted(right),
                original_comparison=dict(
                    diagnostic_only=True,
                    view='ordinary probe comparison; emitted records above use unmodified object bytes',
                    before=dict(bytes_equal=before['bytes_equal'],
                                relocations_equal=before['relocations_equal']),
                    after=dict(bytes_equal=after['bytes_equal'],
                               relocations_equal=after['relocations_equal'])))


def retry_advice(report):
    """Bounded, explicit next actions; candidate imports still validate themselves."""
    if report.get('context_sensitive') is not True:
        return []
    providers = [row['function'] for row in report.get('changed_functions', [])]
    advice = []
    if report['effect'] in ('relocation_fields', 'placement_only'):
        advice.append('Check original extents, bindings and linked relocations; retain original '
                      'scaffolding for unresolved neighbours before rematching the target body.')
    if 'declarations_or_data' in report['context_kinds']:
        advice.append('Resolve the changed prototypes, types, macros and data declarations with '
                      'their defining TU; isolate them before attributing the effect to a callee body.')
    if providers:
        advice.append('Changed same-TU providers: ' + ', '.join(providers) + '. Isolate providers '
                      'one at a time. Reuse published C or import submitted exact_c with '
                      '--dependency-task TASK#FUNCTION; recover an unresolved required group together.')
    if 'inline_or_unmapped_helpers' in report['context_kinds']:
        advice.append('Inspect inline helpers and lexical captures; preserve the original TU '
                      'and registered parent/helper groups.')
    advice.append('Preserve original source order and verify the final composition with the whole-file '
                  'gate, TU audit and independent review. This observation grants no recovery credit.')
    return advice


def retry_evidence(result_path):
    """Return verified diagnostic references for a retry prompt; never import source."""
    path = Path(result_path)
    if not path.is_file() or path.stat().st_size > 8 << 20:
        return []
    result = json.loads(path.read_text())
    if not isinstance(result, dict):
        raise ContextError('context result must be an object')
    references = result.get('context_diagnostics') or []
    if not isinstance(references, list):
        raise ContextError('context diagnostics must be a list')
    verified = []
    for reference in references[-8:]:
        if not isinstance(reference, dict) or not isinstance(reference.get('path'), str):
            raise ContextError('context diagnostic reference is malformed')
        report_path = Path(reference['path'])
        if not report_path.is_file() or report_path.stat().st_size > 1 << 20:
            raise ContextError('a retained context diagnostic is missing or too large')
        raw = report_path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != reference.get('sha256'):
            raise ContextError('a retained context diagnostic differs from its result pin')
        report = json.loads(raw)
        if (not isinstance(report, dict) or
                report.get('schema') != SCHEMA or report.get('acceptance') is not False or
                report.get('task_id') != result.get('task_id') or
                report.get('tu') != (result.get('tu') or {}).get('id') or
                report.get('revision') != (result.get('target') or {}).get('revision') or
                report.get('binary_sha256') != (result.get('target') or {}).get('binary_sha256') or
                report.get('function') not in ((result.get('tu') or {}).get('allocated') or [])):
            raise ContextError('context diagnostic identity differs from its result')
        changes = report.get('changed_functions')
        if (not isinstance(changes, list) or
                any(not isinstance(row, dict) or not isinstance(row.get('function'), str)
                    for row in changes)):
            raise ContextError('context diagnostic providers are malformed')
        if report.get('context_sensitive') is True:
            verified.append(dict(reference, report=report))
    return verified

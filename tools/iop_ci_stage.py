#!/usr/bin/env python3
"""Stage pinned private IOP inputs for the verified objdiff report workflow.

The existing runner caches are the only sources. No toolchain or game input is
downloaded. All source hashes are checked before copying, and destination hashes
are checked afterwards. The IOP backend requires real files inside the checkout.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def destination(root, relative):
    relative = Path(relative)
    require(not relative.is_absolute() and '..' not in relative.parts,
            'invalid pinned destination: ' + str(relative))
    current = root
    for part in relative.parts:
        current = current / part
        require(not current.is_symlink(), 'IOP destination must not be a symlink: ' + str(current))
    return current


def stage(root, toolchain_dir, game_dir):
    root = Path(root).resolve()
    toolchain_dir, game_dir = Path(toolchain_dir).resolve(), Path(game_dir).resolve()
    toolchain = json.loads((root / 'config/iop-toolchains.json').read_text())
    targets = json.loads((root / 'config/iop-targets.json').read_text())
    entries = []
    for pin in toolchain['pins']:
        suffix = Path(pin['path']).relative_to('.work/toolchains')
        entries.append((toolchain_dir / suffix, destination(root, pin['path']), pin['sha256']))
    for target in targets['units'].values():
        entries.append((game_dir / target['path'],
                        destination(root, Path('game/extracted-iso') / target['path']), target['sha256']))
    require(len({target for _, target, _ in entries}) == len(entries), 'duplicate pinned destination')
    # Do not leave partially staged inputs when a cache is missing or stale.
    for source, target, expected in entries:
        require(source.is_file(), 'missing private IOP cache input: ' + str(source))
        require(digest(source) == expected, 'private IOP cache hash mismatch: ' + str(source))
    copied = 0
    for source, target, expected in entries:
        if source.resolve() != target.resolve():
            target.parent.mkdir(parents=True, exist_ok=True)
            destination(root, target.relative_to(root))
            shutil.copy2(source, target)
            copied += 1
        require(target.is_file() and not target.is_symlink() and digest(target) == expected,
                'staged IOP input hash mismatch: ' + str(target))
    return dict(ok=True, verified_inputs=len(entries), copied_inputs=copied,
                toolchain_inputs=len(toolchain['pins']), original_modules=len(targets['units']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--toolchain-dir', default=os.environ.get('XENO_TOOLCHAIN_DIR'))
    parser.add_argument('--game-dir', default=os.environ.get('XENO_GAME_DIR'))
    args = parser.parse_args()
    try:
        require(args.toolchain_dir, 'set XENO_TOOLCHAIN_DIR to the existing private runner tool cache')
        require(args.game_dir, 'set XENO_GAME_DIR to the existing private runner game cache')
        print(json.dumps(stage(args.root, args.toolchain_dir, args.game_dir)))
        return 0
    except (OSError, ValueError, KeyError) as exc:
        print(json.dumps(dict(ok=False, error=str(exc))), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""BridgeLab diagnostic controller. Inspect never executes the imported file."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if (ROOT/'project-root.txt').exists():
    ROOT = Path((ROOT/'project-root.txt').read_text().strip())

def inspect(path):
    p = Path(path).expanduser().resolve(strict=True)
    if not p.is_file():
        raise ValueError('Choose a Windows DLL file, not a folder.')
    with p.open('rb') as f:
        header = f.read(64)
        if len(header) < 64 or header[:2] != b'MZ':
            raise ValueError('Not a Windows PE binary. Unzip the plugin and select its DLL.')
        offset = struct.unpack_from('<I', header, 60)[0]
        if offset < 64 or offset > p.stat().st_size - 26:
            raise ValueError('Invalid PE header offset.')
        f.seek(offset)
        pe = f.read(26)
    if pe[:4] != b'PE\0\0':
        raise ValueError('Invalid PE signature.')
    machine, sections, _, _, _, optional_size, flags = struct.unpack_from('<HHIIIHH', pe, 4)
    magic = struct.unpack_from('<H', pe, 24)[0]
    architectures = {0x14c: ('x86', 32, 0x10b), 0x8664: ('x64', 64, 0x20b), 0xaa64: ('arm64', 64, 0x20b)}
    if machine not in architectures:
        raise ValueError('Unsupported Windows CPU architecture.')
    arch, bits, expected = architectures[machine]
    if magic != expected or optional_size < 2 or not sections or not flags & 0x2000:
        raise ValueError('Invalid or unsupported Windows DLL headers.')
    if offset + 24 + optional_size + sections * 40 > p.stat().st_size:
        raise ValueError('Truncated PE headers.')
    return {'path': str(p), 'architecture': arch, 'bits': bits,
            'format': 'Windows DLL; VST2 entry point not yet verified',
            'status': 'Requires runtime probe; compatibility is unverified'}

def native_command(*args):
    app = ROOT/'BridgeLab.app/Contents/MacOS/BridgeLab'
    if not app.exists():
        raise ValueError('Build or install the complete BridgeLab app first.')
    result = subprocess.run([str(app), *map(str,args)],capture_output=True,text=True,timeout=240)
    if result.returncode:
        raise ValueError((result.stderr+result.stdout)[-4000:])
    return json.loads(result.stdout)

def doctor():
    return native_command('doctor')

def render(path, output):
    return native_command('render',path,output)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('doctor')
    sub.add_parser('inspect').add_argument('plugin')
    sub.add_parser('import').add_argument('plugin')
    r = sub.add_parser('render'); r.add_argument('plugin'); r.add_argument('output')
    args = parser.parse_args()
    try:
        result = doctor() if args.command == 'doctor' else inspect(args.plugin) if args.command == 'inspect' else native_command('import',args.plugin) if args.command == 'import' else render(args.plugin,args.output)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(json.dumps({'error': str(exc)}, indent=2), file=sys.stderr)
        return 1
    return 0
if __name__ == '__main__':
    sys.exit(main())

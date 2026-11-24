#!/usr/bin/env python3
"""Produce a Hub-compatible ZIP only after the actual Hub installer accepts it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import stat
import subprocess
import tempfile
import zipfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def write_archive(source, destination):
    # One application root, no Finder metadata/resource-fork sidecar tree.
    # Store relative symlinks as symlinks so bundled Wine frameworks survive.
    with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=5, allowZip64=False) as archive:
        for path in sorted(source.rglob('*')):
            relative = path.relative_to(source)
            if path.name == '.DS_Store' or '__pycache__' in relative.parts or path.suffix == '.pyc':
                continue
            if path.is_dir() and not path.is_symlink():
                continue
            name = source.name + '/' + relative.as_posix()
            if not name.isascii():
                raise ValueError('Unsupported package path: ' + name)
            if path.is_symlink():
                target = os.readlink(path)
                if target.startswith('/') or not path.resolve().is_relative_to(source.resolve()):
                    raise ValueError('Link escapes application: ' + name)
                info = zipfile.ZipInfo(name)
                info.create_system = 3
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
                archive.writestr(info, target.encode(), compress_type=zipfile.ZIP_STORED)
            else:
                archive.write(path, name)
        if len(archive.infolist()) >= 50000:
            raise ValueError('Archive has too many entries')
    if destination.stat().st_size >= 2_000_000_000:
        raise ValueError('Archive exceeds Hub download limit')


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, default=root / 'BridgeLab.app')
    parser.add_argument('--hub-source', type=Path, default=root.parent / 'StudioHub')
    parser.add_argument('--output', type=Path, default=root / 'release')
    parser.add_argument('--baseline-app', type=Path, default=Path.home() / 'Library/Application Support/ZoneSixStudioHub/Applications/BridgeLab.app')
    args = parser.parse_args()
    app = args.app.resolve()
    hub = args.hub_source.resolve()
    info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
    version = info['CFBundleShortVersionString']
    if app.name != 'BridgeLab.app' or info['CFBundleIdentifier'] != 'local.bridgelab.dragdrop':
        raise ValueError('Expected the signed BridgeLab.app')
    sources = [hub / 'Sources' / name for name in ['Products.swift', 'Archive.swift', 'Installer.swift']]
    if not all(path.is_file() for path in sources):
        raise ValueError('Pass --hub-source pointing to the current StudioHub checkout; the installer gate is required')
    args.output.mkdir(parents=True, exist_ok=True)
    target = args.output / f'BridgeLab-{version}-mac-preview.zip'
    if target.exists():
        raise FileExistsError('Refusing to overwrite an existing release package: ' + str(target))
    with tempfile.TemporaryDirectory(prefix='bridgelab-release-', dir=args.output) as temporary:
        stage = Path(temporary)
        archive = stage / target.name
        write_archive(app, archive)
        checker = stage / 'hub-install-check'
        subprocess.run(['swiftc', '-module-cache-path', str(root / 'build/swift-cache'),
                        *map(str, sources), str(root / 'tests/hub_release_install.swift'),
                        '-o', str(checker), '-framework', 'Cocoa', '-framework', 'CryptoKit'], check=True)
        subprocess.run([str(checker), str(archive), version, str(hub / 'Resources/products.json'),
                        str(args.baseline_app)], check=True)
        report = dict(version=version, filename=target.name, size=archive.stat().st_size,
                      sha256=sha256(archive), hub_installer_passed=True,
                      hub_installer_sources={str(p.relative_to(hub)): sha256(p) for p in sources})
        archive.replace(target)
        (args.output / f'SHA256SUMS-{version}.txt').write_text(report['sha256'] + '  ' + target.name + '\n')
        (args.output / f'package-verification-{version}.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()

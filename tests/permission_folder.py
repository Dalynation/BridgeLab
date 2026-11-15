"""Exercise the native install preflight against real filesystem restrictions."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
results = []
with tempfile.TemporaryDirectory(prefix='bridgelab-permissions-') as td:
    scratch = Path(td)
    main = scratch / 'main.swift'
    main.write_text('''import Foundation
let manager=BridgeManager()
do { print("WRITABLE: "+(try manager.preparePluginDirectory()).path) }
catch { fputs(error.localizedDescription+"\\n",stderr); exit(1) }
''')
    exe = scratch / 'check-folder'
    subprocess.run(['swiftc', '-module-cache-path', str(root/'build/swift-cache'), str(root/'src/Manager.swift'), str(main), '-o', str(exe)], check=True)
    def run(name, folder, expected):
        r = subprocess.run([str(exe)], env=dict(os.environ, BRIDGELAB_PLUGIN_DIR=str(folder)), capture_output=True, text=True)
        assert r.returncode == expected, (name, r.stdout, r.stderr)
        if expected:
            assert all(text in r.stderr for text in ['Read & Write', 'owner=', 'rescan'])
        assert not list(scratch.rglob('.BridgeLab-write-check-*'))
        results.append(dict(test=name, passed=True))
    run('creates missing nested plugin folder', scratch/'new/Plug-Ins/VST3', 0)
    folder = scratch/'denied'
    folder.mkdir()
    sentinel = folder/'OtherVendor.vst3'
    sentinel.write_text('preserve')
    folder.chmod(0o555)
    try:
        run('read-only folder gives recovery instructions', folder, 1)
    finally:
        folder.chmod(0o755)
    assert sentinel.read_text() == 'preserve'
    run('retry after access repair succeeds', folder, 0)
    parent = scratch/'readonly-parent'
    parent.mkdir()
    parent.chmod(0o555)
    try:
        run('restricted parent prevents creation', parent/'VST3', 1)
    finally:
        parent.chmod(0o755)
    blocker = scratch/'not-a-directory'
    blocker.write_text('preserve')
    run('file blocking directory is explained', blocker, 1)
    assert blocker.read_text() == 'preserve'
    subprocess.run(['/bin/chmod', '+a', 'everyone deny add_subdirectory', str(folder)], check=True)
    try:
        run('ACL denial is caught despite writable POSIX mode', folder, 1)
    finally:
        subprocess.run(['/bin/chmod', '-N', str(folder)], check=True)
    assert sentinel.read_text() == 'preserve'
print(json.dumps(results, indent=2))

"""Import and render from an empty, disposable home using only a packaged app."""
import argparse, json, os, struct, subprocess, time, wave
from pathlib import Path
from qa_runtime import stop_profiles

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--app', type=Path, required=True)
p.add_argument('--home', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('plugins', nargs='+', type=Path)
args = p.parse_args()
home = args.home.resolve()
assert not home.exists(), 'A genuinely empty new QA home is required'
home.mkdir(parents=True)
(home / '.bridgelab-qa').touch()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, BRIDGELAB_HOME=str(home), BRIDGELAB_PLUGIN_DIR=str(home / 'wrappers'))
exe = args.app.resolve() / 'Contents/MacOS/BridgeLab'
results = []

def run(label, command, success=True):
    start = time.monotonic()
    process = subprocess.run([str(exe), *map(str, command)], cwd='/private/tmp',
        env=env, capture_output=True, text=True, timeout=300)
    row = dict(test=label, seconds=time.monotonic()-start, exit=process.returncode,
               stdout=process.stdout, stderr=process.stderr,
               passed=(process.returncode == 0) == success)
    results.append(row)
    print(json.dumps(row), flush=True)
    assert row['passed'], label

try:
    for dll in args.plugins:
        run('fresh import ' + dll.name, ['import', dll.resolve()])
        target = out / (dll.stem + '.wav')
        run('packaged render ' + dll.name, ['render', dll.resolve(), target])
        with wave.open(str(target)) as w:
            assert w.getsampwidth() == 2 and w.getnchannels() == 2
            data = w.readframes(w.getnframes())
            peak = max(abs(v[0]) for v in struct.iter_unpack('<h', data))
            assert peak > 32, 'Rendered audio is silent'
        results[-1]['pcm_peak'] = peak
    before = json.loads((home / 'plugins.json').read_text())
    run('idempotent import', ['import', args.plugins[0].resolve()])
    assert len(json.loads((home / 'plugins.json').read_text())) == len(before)
    bad = out / 'invalid.dll'
    bad.write_bytes(b'not a Windows plugin')
    run('invalid DLL rejected', ['import', bad], success=False)
    after = json.loads((home / 'plugins.json').read_text())
    assert after == before, 'Failed or duplicate import changed the registry'
    assert len({r['processor_uid'] for r in after}) == len(args.plugins)
    assert {r['runtime_profile'] for r in after} == {'crossover-shared'}
    results.append(dict(test='independent identities, shared runtime and intact registry', passed=True))
finally:
    cleanup = stop_profiles(home)
    (out / 'results.json').write_text(json.dumps(dict(results=results, cleanup=cleanup), indent=2) + '\n')

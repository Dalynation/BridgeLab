"""Original editor controls during actual Core Audio callbacks.

Use a disposable QA home containing either the slow-editor fixture or the
named TAL test DLLs. Vendor DLLs are not distributed with this test.
"""
from pathlib import Path
import json, math, os, re, struct, subprocess
from qa_runtime import stop_profiles

root = Path(__file__).resolve().parents[1]
home = Path(os.environ['BRIDGELAB_HOME']).resolve()
out = Path(os.environ['BRIDGELAB_QA_OUTPUT']) / 'editor-audio'
assert (home / '.bridgelab-qa').is_file(), 'Use a disposable QA home'
out.mkdir(parents=True, exist_ok=True)
fixtures = {'slow-editor-fixture': 'SLOW', 'TAL-J8X-64': 'J8X',
            'TAL-Pha-64': 'Pha', 'TAL-U-NO-LX-V2-64': 'UNO-LX'}
rows = json.loads((home / 'plugins.json').read_text())
selected = [(row, fixture) for row in rows for name, fixture in fixtures.items()
            if Path(row['path']).stem == name]
assert selected, 'No editor audio fixtures imported'
results = []
try:
    stop_profiles(home)
    for row, fixture in selected:
        for arch in ['arm64', 'x86_64']:
            target = out / (row['id'] + '-' + arch + '.f32')
            log = home / 'logs/host.log'
            offset = log.stat().st_size if log.exists() else 0
            env = dict(os.environ, BRIDGELAB_TEST_EDITOR=fixture,
                       BRIDGELAB_TEST_FRAMES='2880000',
                       BRIDGELAB_TEST_RATE='48000', BRIDGELAB_TEST_BLOCK='64')
            p = subprocess.run(['/usr/bin/arch', '-' + arch,
                                str(root / 'build/vst3/bridge-device'),
                                row['wrapper'], str(target)], env=env,
                               capture_output=True, text=True, timeout=150)
            r = dict(plugin=row['name'], architecture=arch, fixture=fixture,
                     exit=p.returncode, stdout=p.stdout, stderr=p.stderr)
            r['underrun_quanta'] = re.findall(r'underrun quanta=(\d+)', log.read_text()[offset:])
            r['passed'] = False
            if p.returncode == 0:
                raw = target.read_bytes()
                samples = struct.unpack('<' + str(len(raw) // 4) + 'f', raw)
                def rms(a, b):
                    v = samples[int(a * 96000):int(b * 96000)]
                    return math.sqrt(sum(x*x for x in v) / len(v))
                r.update(before_rms=rms(1, 3), after_rms=rms(26, 29), tail_rms=rms(50, 55))
                # GPU editors can take seconds to initialise; the synth must
                # keep producing audio while their original UI is opening.
                r['minimum_loading_block_rms'] = min(
                    math.sqrt(sum(x*x for x in samples[i:i+512]) / 512)
                    for i in range(96000, 8*96000-512, 512))
                r['passed'] = (r['before_rms'] > .001 and
                    r['minimum_loading_block_rms'] > .000001 and
                    r['after_rms'] < r['before_rms'] * .6 and
                    r['tail_rms'] < .00001 and
                    r['underrun_quanta'] == ['0'] and
                    'editor_changed=1' in p.stdout and 'realtime_policy=1' in p.stdout)
            results.append(r)
            (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            print(json.dumps(r), flush=True)
finally:
    (out / 'cleanup.json').write_text(json.dumps(stop_profiles(home), indent=2) + '\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

"""Pure audio queue loss must not manufacture held-note retriggers."""
from pathlib import Path
import argparse, csv, json, os, statistics, subprocess
from qa_runtime import stop_profiles

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('plugin_id')
p.add_argument('--trials', type=int, default=30)
p.add_argument('--arch', choices=['arm64', 'x86_64'], default='x86_64')
p.add_argument('--transport', choices=['shared', 'pipe'], default='shared')
args = p.parse_args()
assert 1 <= args.trials <= 100
root = Path(__file__).resolve().parents[1]
home = Path(os.environ['BRIDGELAB_HOME']).resolve()
assert (home / '.bridgelab-qa').is_file(), 'Use a disposable QA home'
out = Path(os.environ['BRIDGELAB_QA_OUTPUT']) / 'recovery-trace'
out.mkdir(parents=True, exist_ok=True)
row = next(r for r in json.loads((home / 'plugins.json').read_text()) if r['id'] == args.plugin_id)
results = []
try:
    for trial in range(args.trials):
        prefix = out / ('trial-' + str(trial))
        env = dict(os.environ, BRIDGELAB_TIMING_TRACE=str(prefix))
        env.pop('BRIDGELAB_PIPE_AUDIO', None)
        if args.transport == 'pipe':
            env['BRIDGELAB_PIPE_AUDIO'] = '1'
        process = subprocess.run(['/usr/bin/arch', '-' + args.arch,
            str(root / 'build/vst3/bridge-lifecycle-qa'), row['wrapper'],
            'held-key-recovery'], env=env, capture_output=True, text=True, timeout=90)
        events = [event for path in out.glob(prefix.name + '-*.csv')
                  for event in csv.DictReader(path.open())]
        ons = [e for e in events if e['kind'] == 'M' and
               (int(e['a']) >> 16) & 0xf0 == 0x90 and int(e['a']) & 255]
        drops = [e for e in events if e['kind'] == 'D']
        render_us = [int(e['b']) for e in events if e['kind'] == 'R']
        r = dict(trial=trial, architecture=args.arch, transport=args.transport,
                 exit=process.returncode, stdout=process.stdout, stderr=process.stderr,
                 note_ons_submitted=len(ons), rejected_audio_quanta=len(drops),
                 median_render_us=statistics.median(render_us) if render_us else None,
                 passed=process.returncode == 0 and len(ons) == 1 and bool(drops))
        results.append(r)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        print(json.dumps(r), flush=True)
finally:
    (out / 'cleanup.json').write_text(json.dumps(stop_profiles(home), indent=2) + '\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

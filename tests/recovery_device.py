"""Recover a new or already-held key after overload, on Core Audio callbacks."""
from pathlib import Path
import csv, json, math, os, re, struct, subprocess
from qa_runtime import stop_profiles

root = Path(__file__).resolve().parents[1]
home = Path(os.environ['BRIDGELAB_HOME']).resolve()
assert (home / '.bridgelab-qa').is_file(), 'Use a disposable QA home'
rows = json.loads((home / 'plugins.json').read_text())
assert rows, 'Import sounding test instruments'
out = Path(os.environ['BRIDGELAB_QA_OUTPUT']) / 'recovery-device'
out.mkdir(parents=True, exist_ok=True)
results = []
try:
    stop_profiles(home)
    for mode in os.environ.get('BRIDGELAB_RECOVERY_TRANSPORTS', 'shared,pipe').split(','):
        for arch in ['arm64', 'x86_64']:
            for row in rows:
                for key in os.environ.get('BRIDGELAB_RECOVERY_CASES', 'new,held,lost-on').split(','):
                    label = '-'.join([row['id'], mode, arch, key])
                    target = out / (label + '.f32')
                    host_log = home / 'logs/host.log'
                    launch_log = home / 'logs/runner-launch.log'
                    host_offset = host_log.stat().st_size if host_log.exists() else 0
                    launch_offset = launch_log.stat().st_size if launch_log.exists() else 0
                    env = dict(os.environ, BRIDGELAB_TEST_OVERLOAD=key,
                               BRIDGELAB_TEST_FRAMES='480000',
                               BRIDGELAB_TEST_RATE='48000', BRIDGELAB_TEST_BLOCK='64')
                    env.pop('BRIDGELAB_PIPE_AUDIO', None)
                    if mode == 'pipe':
                        env['BRIDGELAB_PIPE_AUDIO'] = '1'
                    if key.startswith('lost-'):
                        env['BRIDGELAB_TIMING_TRACE'] = str(out / label)
                    p = subprocess.run(['/usr/bin/arch', '-' + arch,
                        os.environ.get('BRIDGELAB_DEVICE_HOST', str(root / 'build/vst3/bridge-device')), row['wrapper'],
                        str(target)], env=env, capture_output=True, text=True, timeout=90)
                    r = dict(plugin=row['name'], architecture=arch, transport=mode,
                             key=key, exit=p.returncode, stdout=p.stdout, stderr=p.stderr,
                             passed=False)
                    host_text = host_log.read_text()[host_offset:]
                    launch_text = launch_log.read_text()[launch_offset:]
                    r['transport_confirmed'] = re.findall(r'transport=(\w+)', host_text) == [mode]
                    r['dsp_realtime_confirmed'] = ('scheduler=1' in host_text if mode == 'shared'
                        else bool(re.search(r'BridgeLab pipe DSP pid=\d+ realtime_policy=1', launch_text)))
                    r['io_realtime_confirmed'] = mode == 'shared' or 'io_scheduler=1' in host_text
                    r['lost_midi_confirmed'] = not key.startswith('lost-')
                    if key.startswith('lost-'):
                        trace = [event for path in out.glob(label+'-*.csv')
                                 for event in csv.DictReader(path.open())]
                        dropped_seq = 543 if key == 'lost-off' else 511
                        r['lost_midi_confirmed'] = any(e['kind']=='D' and int(e['seq'])==dropped_seq and int(e['a'])==1 for e in trace)
                    if p.returncode == 0:
                        raw = target.read_bytes()
                        samples = struct.unpack('<' + str(len(raw)//4) + 'f', raw)
                        # Ignore the intentional overload and initial settling.
                        # Every subsequent 64-frame block must carry the held
                        # note; another note-on cannot mask held-key failure.
                        values = [math.sqrt(sum(x*x for x in samples[i:i+128])/128)
                                  for i in range(96000, 4*96000, 128)]
                        r['minimum_recovered_block_rms'] = min(values)
                        r['maximum_recovered_block_rms'] = max(values)
                        audio_ok = max(values) == 0 if key == 'lost-off' else min(values) > .000001
                        if key.startswith('lost-'):
                            latency = int(re.search(r'latency=(\d+)', p.stdout).group(1)) // 256
                            played = {int(e['seq']) for e in trace if e['kind']=='A'}
                            expected = range(dropped_seq+1+48000//256-latency,
                                             dropped_seq+1+4*48000//256-latency)
                            r['missing_recovered_quanta'] = sum(n not in played for n in expected)
                            audio_ok = audio_ok and r['missing_recovered_quanta'] == 0
                        if key == 'lost-off':
                            peak = re.search(r'overload_initial_peak=([\d.eE+-]+)', p.stdout)
                            r['initial_peak'] = float(peak.group(1)) if peak else 0
                            r['explicit_noteoff_submitted'] = any(e['kind']=='M' and
                                dropped_seq<int(e['seq'])<dropped_seq+1+48000//256-latency and
                                (int(e['a'])>>16)&0xf0==0x80 and (int(e['a'])>>8)&127==60
                                for e in trace)
                            audio_ok = audio_ok and r['initial_peak']>.001 and r['explicit_noteoff_submitted']
                        r['passed'] = (r['transport_confirmed'] and r['dsp_realtime_confirmed'] and r['io_realtime_confirmed'] and
                                       r['lost_midi_confirmed'] and audio_ok and
                                       'realtime_policy=1' in p.stdout)
                    results.append(r)
                    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                    print(json.dumps(r), flush=True)
finally:
    (out / 'cleanup.json').write_text(json.dumps(stop_profiles(home), indent=2) + '\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

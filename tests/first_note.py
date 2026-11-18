"""Check first-note audio for explicitly selected, playable factory patches.

BRIDGELAB_HOME must point to a disposable QA home. Pass plugin IDs, not names;
the same check applies to any installed instrument with a sounding default.
Empty sampler patches are not suitable fixtures for this check.
"""
import argparse
import json
import struct
import time
from pathlib import Path

from installed_editor import HOME, Session
from qa_runtime import stop_profiles


def first_note(session, note):
    peak = 0.0
    for index in range(180):
        midi = struct.pack('<I4B', 0, 0x90, note, 100, 0) if index == 0 else b''
        session.p.stdin.write(struct.pack('<6I', 0x424c3032, 256,
                                          int(bool(midi)), 0, 0, 0)
                              + midi + bytes(1024 - len(midi)))
        session.p.stdin.flush()
        raw = session.read(2064)
        assert struct.unpack_from('<I', raw)[0] == 0x424c3032
        peak = max(peak, *(abs(x) for x in struct.unpack_from('<512f', raw, 16)))
        time.sleep(.005)
    return peak


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ids', nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert (HOME / '.bridgelab-qa').exists(), 'Use a marked disposable QA home'
    rows = {row['id']: row for row in json.loads((HOME / 'plugins.json').read_text())}
    results = []
    try:
        for plugin_id in args.ids:
            row = rows[plugin_id]
            for scenario in ('cold', 'restore', 'select-program'):
                result = dict(id=plugin_id, plugin=row['name'], scenario=scenario)
                session = None
                try:
                    session = Session(row)
                    if scenario == 'restore':
                        _, state = session.request(3)
                        assert session.request(4, state)[0][2] == 1
                    elif scenario == 'select-program':
                        assert session.request(9, program=0)[0][2] == 1
                    result['peak'] = first_note(session, 60)
                    result['passed'] = result['peak'] > 1e-6
                except Exception as error:
                    result.update(passed=False, error=str(error))
                finally:
                    if session:
                        session.close()
                results.append(result)
                print(json.dumps(result), flush=True)
    finally:
        stop_profiles(HOME)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(results, indent=2) + '\n')
    return 0 if results and all(result['passed'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())

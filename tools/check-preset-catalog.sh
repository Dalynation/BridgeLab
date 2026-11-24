#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
swiftc -module-cache-path build/swift-cache src/Manager.swift tests/program_names.swift -o build/program-names-test
build/program-names-test
python3 - <<'PY'
from pathlib import Path
import shutil
source=Path('BridgeLabInstrument.vst3')
fixture=Path('build/program-catalog-test.vst3')
if fixture.exists():shutil.rmtree(fixture)
shutil.copytree(source,fixture,symlinks=True)
(fixture/'Contents/Resources/programs.txt').write_text(''.join(n.encode().hex()+'\n' for n in ['First preset','Second preset','Third preset']))
PY
codesign --force --sign - build/program-catalog-test.vst3
/usr/bin/arch -arm64 build/vst3/bridge-programs build/program-catalog-test.vst3
/usr/bin/arch -x86_64 build/vst3/bridge-programs build/program-catalog-test.vst3

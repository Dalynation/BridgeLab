#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
workspace_root="$(cd ../.. && pwd)"
toolchain="$workspace_root/work/llvm-mingw-20261104-ucrt-macos-universal/bin"
mkdir -p build
"$toolchain/i686-w64-mingw32-clang++" -std=c++17 -O2 -static src/runner.cpp -o build/runner-x86-v14.exe -luser32 -lgdi32 -lwinmm -lole32
"$toolchain/x86_64-w64-mingw32-clang++" -std=c++17 -O2 -static src/runner.cpp -o build/runner-x64-v14.exe -luser32 -lgdi32 -lwinmm -lole32

for arch in i686 x86_64; do
 "$toolchain/$arch-w64-mingw32-clang++" -std=c++17 -O2 -static tests/graphics_contract.cpp -o "build/graphics-test-$arch.exe" -luser32 -lgdi32 -lwinmm -lole32
 "$toolchain/$arch-w64-mingw32-clang++" -std=c++17 -O2 -static tests/popup_contract.cpp -o "build/popup-test-$arch.exe" -luser32 -lgdi32
 "$toolchain/$arch-w64-mingw32-clang++" -std=c++17 -O2 -static tests/menu_contract.cpp -o "build/menu-test-$arch.exe" -luser32 -lgdi32
 "$toolchain/$arch-w64-mingw32-clang++" -std=c++17 -O2 -static tests/input_capture.cpp -o "build/input-capture-test-$arch.exe" -luser32 -lgdi32
 "$toolchain/$arch-w64-mingw32-clang++" -std=c++17 -O2 -static tests/audio_health.cpp -o "build/audio-health-test-$arch.exe"
done

"$toolchain/x86_64-w64-mingw32-clang++" -std=c++17 -O2 -static -shared tests/slow_editor_fixture.cpp -o build/slow-editor-fixture.dll -luser32 -lgdi32
"$toolchain/x86_64-w64-mingw32-clang++" -std=c++17 -O2 -static -shared -DBRIDGELAB_QUEUE_FIXTURE tests/slow_editor_fixture.cpp -o build/queue-loss-fixture.dll -luser32 -lgdi32

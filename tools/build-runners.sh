#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
workspace_root="$(cd ../.. && pwd)"
toolchain="$workspace_root/work/llvm-mingw-20261104-ucrt-macos-universal/bin"
mkdir -p build
"$toolchain/i686-w64-mingw32-clang++" -std=c++17 -O2 -static src/runner.cpp -o build/runner-x86-v14.exe -luser32 -lgdi32 -lwinmm
"$toolchain/x86_64-w64-mingw32-clang++" -std=c++17 -O2 -static src/runner.cpp -o build/runner-x64-v14.exe -luser32 -lgdi32 -lwinmm

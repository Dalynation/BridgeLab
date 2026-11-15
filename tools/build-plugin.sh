#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
project_root="$PWD"
workspace_root="$(cd ../.. && pwd)"
cmake_bin="$workspace_root/work/python-tools/cmake/data/bin/cmake"
sdk_root="$workspace_root/work/vst3sdk"
# The SDK queries xcodebuild even for Makefile builds. This machine uses CLT 26.
XCODE_VERSION=26 "$cmake_bin" -S . -B build/vst3 -DXCODE_VERSION=26 -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DVST3_SDK_ROOT="$sdk_root"
"$cmake_bin" --build build/vst3 -j 6
codesign --force --sign - build/vst3/VST3/Release/BridgeLabInstrument.vst3
ditto build/vst3/VST3/Release/BridgeLabInstrument.vst3 BridgeLabInstrument.vst3

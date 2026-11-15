#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build 'BridgeLab.app/Contents/MacOS'
clang++ -std=c++17 -Wall -Wextra -Werror src/runner.cpp -o build/runner-native
clang++ -std=c++17 -Wall -Wextra -Werror -dynamiclib tests/fixture.cpp -o build/fixture.dylib
swiftc -module-cache-path build/swift-cache -target arm64-apple-macos11.0 src/App.swift src/Manager.swift -o build/app-arm64 -framework Cocoa
swiftc -module-cache-path build/swift-cache -target x86_64-apple-macos11.0 src/App.swift src/Manager.swift -o build/app-x86_64 -framework Cocoa
lipo -create build/app-arm64 build/app-x86_64 -output BridgeLab.app/Contents/MacOS/BridgeLab
cat > BridgeLab.app/Contents/Info.plist <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict><key>CFBundleVersion</key><string>91</string><key>CFBundleShortVersionString</key><string>0.9.1</string><key>CFBundleDocumentTypes</key><array><dict><key>CFBundleTypeName</key><string>Windows Plugin DLL</string><key>CFBundleTypeRole</key><string>Viewer</string><key>LSHandlerRank</key><string>Alternate</string><key>CFBundleTypeExtensions</key><array><string>dll</string></array></dict></array><key>CFBundleIconFile</key><string>BridgeLab</string><key>CFBundleExecutable</key><string>BridgeLab</string><key>CFBundleIdentifier</key><string>local.bridgelab.dragdrop</string><key>CFBundleName</key><string>BridgeLab</string><key>CFBundlePackageType</key><string>APPL</string><key>NSHighResolutionCapable</key><true/></dict></plist>
PLIST
python3 tools/package-engine.py
swiftc -module-cache-path build/swift-cache tools/make-icon.swift -o build/make-icon -framework Cocoa
build/make-icon build/BridgeLab.iconset
python3 tools/package-icon.py
if [ -x build/vst3/bridge-editor-attachment ]; then
  /usr/bin/arch -arm64 build/vst3/bridge-editor-attachment "$PWD/BridgeLab.app/Contents/Resources/Engine/libBridgeLabUI.dylib"
  /usr/bin/arch -x86_64 build/vst3/bridge-editor-attachment "$PWD/BridgeLab.app/Contents/Resources/Engine/libBridgeLabUI.dylib"
fi
codesign --force --sign - BridgeLab.app
python3 -m unittest discover -s tests -v

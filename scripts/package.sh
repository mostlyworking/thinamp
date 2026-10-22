#!/bin/bash
# Builds a universal (Apple Silicon + Intel) release and zips it with the installer.
# Output: dist/Thinamp-mac.zip, ready to attach to a GitHub release.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-release --target Thinamp_VST3 Thinamp_AU EngineTests
./build-release/EngineTests_artefacts/Release/EngineTests

artefacts=build-release/Thinamp_artefacts/Release
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

ditto "$artefacts/VST3/Thinamp.vst3" "$stage/Thinamp.vst3"
ditto "$artefacts/AU/Thinamp.component" "$stage/Thinamp.component"
cp install.sh "$stage/Install Thinamp.command"
chmod +x "$stage/Install Thinamp.command"

lipo -archs "$stage/Thinamp.vst3/Contents/MacOS/Thinamp"

mkdir -p dist
rm -f dist/Thinamp-mac.zip
ditto -c -k --sequesterRsrc "$stage" dist/Thinamp-mac.zip
echo "Wrote dist/Thinamp-mac.zip"

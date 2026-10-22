#!/bin/bash
# Builds a universal (Apple Silicon + Intel) release and zips it with the installer.
# Output: dist/FourierTruncator-mac.zip, ready to attach to a GitHub release.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-release --target FourierTruncator_VST3 FourierTruncator_AU EngineTests
./build-release/EngineTests_artefacts/Release/EngineTests

artefacts=build-release/FourierTruncator_artefacts/Release
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

ditto "$artefacts/VST3/Fourier Truncator.vst3" "$stage/Fourier Truncator.vst3"
ditto "$artefacts/AU/Fourier Truncator.component" "$stage/Fourier Truncator.component"
cp install.sh "$stage/Install Fourier Truncator.command"
chmod +x "$stage/Install Fourier Truncator.command"

lipo -archs "$stage/Fourier Truncator.vst3/Contents/MacOS/Fourier Truncator"

mkdir -p dist
rm -f dist/FourierTruncator-mac.zip
ditto -c -k --sequesterRsrc "$stage" dist/FourierTruncator-mac.zip
echo "Wrote dist/FourierTruncator-mac.zip"

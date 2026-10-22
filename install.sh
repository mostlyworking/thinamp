#!/bin/bash
# Installs Thinamp (VST3 + AU) for the current user. No admin password or developer tools needed.
#   Online:  curl -fsSL https://raw.githubusercontent.com/mostlyworking/thinamp/main/install.sh | bash
#   Offline: double-click "Install Thinamp.command" inside the release zip.
set -euo pipefail

url="https://github.com/mostlyworking/thinamp/releases/latest/download/Thinamp-mac.zip"
vst3_dir="$HOME/Library/Audio/Plug-Ins/VST3"
au_dir="$HOME/Library/Audio/Plug-Ins/Components"

# Use the plugins next to this script when run from the unzipped release; otherwise download them.
src=""
if [[ -n "${BASH_SOURCE[0]:-}" && -f "${BASH_SOURCE[0]}" ]]; then
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    [[ -d "$here/Thinamp.vst3" ]] && src="$here"
fi

if [[ -z "$src" ]]; then
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT
    echo "Downloading Thinamp..."
    curl -fL --progress-bar "$url" -o "$tmp/ft.zip"
    ditto -x -k "$tmp/ft.zip" "$tmp/unzipped"
    src="$tmp/unzipped"
fi

echo "Installing..."
mkdir -p "$vst3_dir" "$au_dir"
# Remove copies installed under the plugin's old name (same plugin ID, would show up twice).
rm -rf "$vst3_dir/Fourier Truncator.vst3" "$au_dir/Fourier Truncator.component"
rm -rf "$vst3_dir/Thinamp.vst3" "$au_dir/Thinamp.component"
ditto "$src/Thinamp.vst3" "$vst3_dir/Thinamp.vst3"
ditto "$src/Thinamp.component" "$au_dir/Thinamp.component"

# The plugin isn't notarized by Apple; clearing the download flag stops macOS from blocking it.
xattr -dr com.apple.quarantine "$vst3_dir/Thinamp.vst3" "$au_dir/Thinamp.component" 2>/dev/null || true

# Ask macOS to re-scan Audio Units.
killall -9 AudioComponentRegistrar 2>/dev/null || true

cat <<'DONE'

Thinamp is installed.

Restart your DAW (or rescan plugins) and look for "Thinamp".

Ableton Live: Settings > Plug-Ins, turn on "Use VST3 Plug-In System Folders", click Rescan.
Other DAWs: https://github.com/mostlyworking/thinamp#finding-it-in-your-daw

DONE

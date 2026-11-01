#!/usr/bin/env sh
# Offscreen screenshots of every workspace, ffgui's and (when a Quilt checkout
# is around) the mock-up's, side by side in one folder.
#
#   scripts/screenshots.sh [out dir] [quilt checkout]
#
# Defaults: build/screenshots and ~/Development/quilt.
set -eu
cd "$(dirname "$0")/.."
out="$(realpath -m "${1:-build/screenshots}")"
quilt="${2:-$HOME/Development/quilt}"
preset="${FFGUI_PRESET:-dev}"
mkdir -p "$out"

export QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1
XDG_CONFIG_HOME="$(mktemp -d)"
export XDG_CONFIG_HOME
trap 'rm -rf "$XDG_CONFIG_HOME"' EXIT

# Capped like the tests, so nothing here can take over the machine.
capped() {
    if systemd-run --user --scope --quiet true 2>/dev/null; then
        systemd-run --user --scope --quiet -p MemoryMax=2G -p MemorySwapMax=0 -p CPUQuota=200% "$@"
    else
        "$@"
    fi
}

cmake --build --preset "$preset" --target tst_qml >/dev/null
FFGUI_SCREENSHOT_DIR="$out" capped timeout 180 "build/$preset/tests/qml/tst_qml" -input tests/qml/tst_workspaces.qml >/dev/null

if [ -f "$quilt/examples/ffgui/Main.qml" ]; then
    qml_bin="$(command -v qml6 || command -v qml || echo "$(qmake6 -query QT_INSTALL_BINS)/qml")"
    capped timeout 60 "$qml_bin" -I "$quilt" scripts/mockup-shots.qml -- "file://$quilt/examples/ffgui/Main.qml" "$out" 2>/dev/null
fi
ls "$out"

#!/usr/bin/env sh
# Copies the Quilt module from a Quilt checkout into third_party/Quilt, records
# which commit it came from, then rebuilds and runs the tests.
#
#   scripts/sync-quilt.sh                      # from ~/Development/quilt
#   scripts/sync-quilt.sh path/to/quilt        # from another checkout
#   scripts/sync-quilt.sh --no-build [path]    # copy only
#
# third_party/Quilt is never edited by hand. When a sync breaks something, the
# fix belongs in qml/Shell (DockShell, WorkspaceController, PanelsMenu) or in
# the FFgui.Controls facade; see CLAUDE.md.
set -eu
cd "$(dirname "$0")/.."

build=1
if [ "${1:-}" = "--no-build" ]; then
    build=0
    shift
fi
src="${1:-$HOME/Development/quilt}"

if [ ! -f "$src/Quilt/qmldir" ]; then
    echo "sync-quilt.sh: $src/Quilt/qmldir not found; pass the path to a Quilt checkout" >&2
    exit 1
fi

rm -rf third_party/Quilt
mkdir -p third_party
cp -R "$src/Quilt" third_party/Quilt

commit="$(git -C "$src" rev-parse HEAD 2>/dev/null || echo unknown)"
subject="$(git -C "$src" log -1 --format=%s 2>/dev/null || echo unknown)"
dirty="$(git -C "$src" status --porcelain -- Quilt 2>/dev/null | head -n 1)"
{
    echo "source:  $src"
    echo "commit:  $commit${dirty:+ (plus uncommitted changes in Quilt/)}"
    echo "subject: $subject"
    echo "copied:  $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > third_party/QUILT_VERSION
cat third_party/QUILT_VERSION

if [ "$build" = 1 ]; then
    if [ ! -d build/dev ]; then
        cmake --preset dev
    fi
    scripts/run-tests.sh
fi

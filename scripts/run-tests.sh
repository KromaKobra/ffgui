#!/usr/bin/env sh
# Builds, then runs the import-boundary check, qmllint over every module and
# the test suite (ctest, offscreen, with a throwaway settings directory).
#
#   scripts/run-tests.sh                 # the dev preset
#   FFGUI_PRESET=release scripts/run-tests.sh
#   scripts/run-tests.sh -R qml          # extra arguments go to ctest
set -eu
cd "$(dirname "$0")/.."
preset="${FFGUI_PRESET:-dev}"

if [ ! -d "build/$preset" ]; then
    cmake --preset "$preset"
fi
cmake --build --preset "$preset"

echo "== import boundaries"
scripts/check-imports.sh

echo "== qmllint"
lint_log="$(mktemp)"
trap 'rm -f "$lint_log"' EXIT
if ! cmake --build --preset "$preset" --target all_qmllint >"$lint_log" 2>&1; then
    cat "$lint_log"
    echo "qmllint failed" >&2
    exit 1
fi
if grep -E '^(Warning|Error|Critical):' "$lint_log" >/dev/null; then
    grep -E -A3 '^(Warning|Error|Critical):' "$lint_log"
    echo "qmllint reported warnings" >&2
    exit 1
fi
echo "qmllint clean"

echo "== ctest"
# Qt may log to the journal instead of the terminal; force stderr. Tests get
# their own settings directory so workspace and layout tests never touch yours.
export QT_FORCE_STDERR_LOGGING=1
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM_TESTS:-offscreen}"
XDG_CONFIG_HOME="$(mktemp -d)"
export XDG_CONFIG_HOME
trap 'rm -f "$lint_log"; rm -rf "$XDG_CONFIG_HOME"' EXIT
ctest --preset "$preset" "$@"

#!/usr/bin/env sh
# Enforces the QML layering rules from CLAUDE.md by reading import lines.
#
#   Theme <- Controls <- feature modules <- Shell
#
# - FFgui.Theme imports nothing of ours.
# - FFgui.Controls imports only FFgui.Theme (no Backend, so it stays reusable).
# - Feature modules may import Backend, Theme and Controls, never Quilt and
#   never each other, and use the Ff* input controls instead of QtQuick.Controls
#   inputs.
# - Only DockShell, WorkspaceController and PanelsMenu import Quilt.
set -eu
cd "$(dirname "$0")/.."

status=0
fail() {
    echo "check-imports: $1" >&2
    status=1
}

imports() {
    # The module names a file imports, one per line.
    sed -n 's/^[[:space:]]*import[[:space:]]\{1,\}\([A-Za-z][A-Za-z0-9.]*\).*/\1/p' "$1"
}

for f in qml/Theme/*.qml qml/Theme/*.js; do
    [ -e "$f" ] || continue
    for m in $(imports "$f"); do
        case "$m" in
            FFgui*|Quilt) fail "$f imports $m (Theme must not import other ffgui modules)" ;;
        esac
    done
done

for f in qml/Controls/*.qml qml/Controls/*.js; do
    [ -e "$f" ] || continue
    for m in $(imports "$f"); do
        case "$m" in
            FFgui.Theme) ;;
            FFgui*|Quilt) fail "$f imports $m (Controls may only import FFgui.Theme)" ;;
        esac
    done
done

feature_files="$(find qml/Sources qml/Media qml/Meters qml/Chain qml/Inspector qml/Output qml/Tools \
    -name '*.qml' -o -name '*.js' 2>/dev/null | sort)"
for f in $feature_files; do
    own="$(echo "$f" | sed 's#^qml/##; s#/[^/]*$##; s#/#.#g')"
    for m in $(imports "$f"); do
        case "$m" in
            Quilt) fail "$f imports Quilt (only the Shell talks to the dock)" ;;
            FFgui.Backend|FFgui.Theme|FFgui.Controls|"FFgui.$own") ;;
            FFgui.Shell) fail "$f imports FFgui.Shell (use App.requestPanel)" ;;
            FFgui*) fail "$f imports $m (feature modules never import each other)" ;;
        esac
    done
    # Inputs come from the FFgui.Controls facade, so Quilt's own controls can
    # replace them in one place later.
    if grep -nE '^[[:space:]]*(Button|ToolButton|CheckBox|Switch|Slider|RangeSlider|ComboBox|TextField|TextArea|SpinBox|RadioButton|Menu|MenuItem|MenuSeparator|ItemDelegate|Dial|DelayButton|RoundButton)[[:space:]]*\{' "$f" >/dev/null; then
        grep -nE '^[[:space:]]*(Button|ToolButton|CheckBox|Switch|Slider|RangeSlider|ComboBox|TextField|TextArea|SpinBox|RadioButton|Menu|MenuItem|MenuSeparator|ItemDelegate|Dial|DelayButton|RoundButton)[[:space:]]*\{' "$f" | sed "s#^#$f:#" >&2
        fail "$f uses QtQuick.Controls inputs directly (use the Ff* controls)"
    fi
done

for f in $(grep -rlE '^[[:space:]]*import[[:space:]]+Quilt' qml 2>/dev/null); do
    case "$f" in
        qml/Shell/DockShell.qml|qml/Shell/WorkspaceController.qml|qml/Shell/PanelsMenu.qml) ;;
        *) fail "$f imports Quilt (only DockShell, WorkspaceController and PanelsMenu may)" ;;
    esac
done

if [ "$status" = 0 ]; then
    echo "import boundaries ok"
fi
exit "$status"

pragma Singleton
import QtQuick

// Every icon ffgui draws, by name. They are Unicode characters for now, so
// swapping in an icon font or SVGs later touches only this file.
QtObject {
    // Transport
    readonly property string play: "▶"
    readonly property string pause: "⏸"
    readonly property string stop: "⏹"
    readonly property string previous: "⏮"
    readonly property string next: "⏭"
    readonly property string loop: "⟲"
    readonly property string speaker: "🔈"

    // Actions
    readonly property string add: "+"
    readonly property string close: "✕"
    readonly property string remove: "×"
    readonly property string undo: "↶"
    readonly property string redo: "↷"
    readonly property string reset: "↺"
    readonly property string expand: "⤢"
    readonly property string search: "⌕"
    readonly property string more: "⋯"

    // Disclosure
    readonly property string open: "▾"
    readonly property string closed: "▸"
    readonly property string up: "▴"
    readonly property string arrow: "→"
    readonly property string flowArrow: "▶"
    readonly property string range: "⟷"

    // Status
    readonly property string warning: "⚠"
    readonly property string check: "✓"
    readonly property string cross: "✕"
    readonly property string star: "★"
    readonly property string dot: "●"
    readonly property string phase: "φ"

    // Input kinds
    readonly property string file: "♫"
    readonly property string generator: "∿"
    readonly property string folder: "▣"
    readonly property string device: "●"
    readonly property string url: "◍"

    function inputKind(kind) {
        return kind === "generator" ? generator : kind === "folder" ? folder
             : kind === "device" ? device : kind === "url" ? url : file;
    }
}

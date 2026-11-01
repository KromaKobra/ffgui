pragma Singleton
import QtQuick

// Colour tokens. Every colour in ffgui comes from here; components never use
// hex literals. Names say what a colour is for, not what it looks like.
QtObject {
    // ----- Surfaces, darkest to lightest
    readonly property color statusBar: "#0d0f12"
    readonly property color canvas: "#0c0e11"           // Media View, scopes
    readonly property color well: "#101215"             // meters, sunken areas
    readonly property color field: "#0f1114"            // command preview
    readonly property color window: "#111317"           // behind the panels
    readonly property color chainBackground: "#14161a"
    readonly property color laneLabel: "#14171b"
    readonly property color base: "#15171b"             // text field fill
    readonly property color chrome: "#16181c"           // header, tab bar
    readonly property color outputBar: "#181a1f"
    readonly property color footer: "#191b20"
    readonly property color stripe: "#1a1d22"           // alternate rows
    readonly property color row: "#1d2026"
    readonly property color panel: "#1e2127"
    readonly property color chip: "#1f2228"
    readonly property color card: "#23262d"
    readonly property color cardCurrent: "#22283a"
    readonly property color hover: "#2a2e36"
    readonly property color control: "#2a2e36"          // buttons
    readonly property color selected: "#263552"
    readonly property color ioBlock: "#1b2433"          // Input and Output blocks in the Chain

    // ----- Lines
    readonly property color line: "#262a31"
    readonly property color divider: "#2a2e36"
    readonly property color border: "#30343c"
    readonly property color cardBorder: "#2c3038"
    readonly property color cardBorderCurrent: "#33405e"
    readonly property color blockBorder: "#353a43"
    readonly property color borderStrong: "#3a3f48"
    readonly property color dashed: "#4a505b"
    readonly property color ioBlockBorder: "#2f3d55"

    // ----- Text
    readonly property color text: "#d7dae0"
    readonly property color textSoft: "#b8bec9"
    readonly property color textMuted: "#9aa3b2"
    readonly property color textDim: "#7f8591"
    readonly property color textFaint: "#5d6470"
    readonly property color textBright: "#f5f7fa"
    readonly property color textOnAccent: "#ffffff"
    readonly property color textOnWarn: "#1b1b1b"
    readonly property color ioLabel: "#6f8bb8"

    // ----- Accent
    readonly property color accent: "#4c8dff"
    readonly property color accentSoft: "#8db6ff"
    readonly property color accentGlyph: "#6fa8ff"
    readonly property color accentDeep: "#2a3f66"
    readonly property color wire: "#5b7fb8"
    readonly property color accentFill: Qt.rgba(0.30, 0.55, 1.0, 0.28)

    // ----- Status
    readonly property color ok: "#46c37b"
    readonly property color okDeep: "#2f7d4f"
    readonly property color warn: "#e0a43a"
    readonly property color error: "#ef5350"
    readonly property color errorSoft: "#ef8a80"
    readonly property color errorDeep: "#c94141"
    readonly property color neutral: "#3a3f48"          // neutral badge

    // ----- Domain
    readonly property color command: "#a9d1ff"          // ffmpeg command and filter text
    readonly property color wavePeak: "#3d82cc"
    readonly property color waveRms: "#8fcbff"
    readonly property color waveGhost: Qt.rgba(0.63, 0.67, 0.75, 0.22)   // the original under the processed result
    readonly property color waveCentre: Qt.rgba(1, 1, 1, 0.12)
    readonly property color waveOriginal: "#4b6a8f"
    readonly property color overview: "#3b4a5e"
    readonly property color playhead: "#f5f7fa"
    readonly property color crosshair: "#ffffff"
    readonly property color tooltip: "#000000"
    readonly property color selectionFill: Qt.rgba(0.30, 0.55, 1.0, 0.18)
    readonly property color selectionEdge: "#8db6ff"
    readonly property color selectionLabel: "#2a3f66"
    readonly property color silence: Qt.rgba(0.50, 0.52, 0.57, 0.16)
    readonly property color cut: Qt.rgba(0, 0, 0, 0.55)
    readonly property color rangeFill: Qt.rgba(0.30, 0.55, 1.0, 0.08)
    readonly property color grid: Qt.rgba(1, 1, 1, 0.06)
    readonly property color chapter: "#e0a43a"
    readonly property color clip: "#ef5350"
    readonly property color dcLine: "#e0a43a"
    readonly property color loudness: "#7ee0a8"
    readonly property color loudnessBadge: "#1d3a2a"
    readonly property color target: "#e0a43a"
    readonly property color phaseGood: "#2f7d4f"
    readonly property color phaseWarn: "#e0a43a"
    readonly property color phaseBad: "#ef5350"
    readonly property color transcript: "#22303f"
    readonly property color transcriptText: "#a9c4e0"
    readonly property color vectorscope: Qt.rgba(0.49, 0.88, 0.66, 0.65)
    readonly property color histogram: "#5b7fb8"
    readonly property color meterTrack: "#1a1d22"

    // Level meter gradient, top (0 dBFS) to bottom.
    readonly property color meterHot: "#ef5350"
    readonly property color meterWarm: "#e0a43a"
    readonly property color meterGood: "#46c37b"
    readonly property color meterLow: "#2f7d4f"

    // Noise colours for the Generate panel.
    readonly property var noise: ({
        white: "#f0f0f0", pink: "#f29bb8", brown: "#9c6b44", blue: "#6aa0ff", violet: "#a77bea"
    })

    // Spectrogram colour map (magma-like): [position, r, g, b] stops.
    readonly property var spectrogramStops: [[0, 0, 0, 4], [0.25, 59, 15, 112], [0.5, 140, 41, 129],
                                             [0.7, 222, 73, 104], [0.85, 254, 159, 109], [1, 252, 253, 191]]

    // Health dot and status colours by key.
    function health(state) {
        return state === "ok" ? ok : state === "warn" ? warn : state === "error" ? error : textFaint;
    }
}

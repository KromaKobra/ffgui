.pragma library

// ffgui's workspaces: saved arrangements for common jobs. Each one is a Quilt
// layout (the same JSON that DockArea.saveLayout() produces) plus the view
// settings that belong to it (Media View layers, spectrogram size, Chain
// graph mode). Switching changes only the arrangement; the project stays.
//
// Core panels (sources, media, meters, chain, inspector) can't be closed, so
// every layout places all of them. Toggleable panels a layout leaves out stay
// closed, because DockShell declares them with openOnStart: false.
//
// Ported unchanged from Quilt's ffgui mock-up (examples/ffgui/Workspaces.js).

var names = ["Default", "Convert", "Inspect", "Edit", "Master", "Restore", "Record & Stream", "Batch"];

var descriptions = {
    "Default": "The standard layout: four stages left to right",
    "Convert": "Format changes and batch conversions",
    "Inspect": "Quality checks, loudness compliance, finding problems",
    "Edit": "Trimming, fades, joining, chapters, tags",
    "Master": "EQ, dynamics, loudness targets, A/B listening",
    "Restore": "Noise, click and hum removal guided by the spectrogram",
    "Record & Stream": "Capture and live broadcasting",
    "Batch": "One recipe applied to many files"
};

// The centre column: Media View with the Meters strip on its right edge,
// and the Chain below. `mediaShare` is the Media View row's part of the
// column height.
function centre(mediaShare, metersSize) {
    return { column: [
        { row: [{ tabs: ["media"] }, { tabs: ["meters"], size: metersSize || 0.08 }], size: mediaShare },
        { tabs: ["chain"] }
    ] };
}

function sized(node, size) {
    node.size = size;
    return node;
}

var layouts = {
    "Default": { row: [
        { tabs: ["sources"], size: 0.2 },
        centre(0.66),
        { tabs: ["inspector"], size: 0.22 }
    ] },
    "Convert": { row: [
        { tabs: ["sources"], size: 0.36 },
        centre(0.82),
        { tabs: ["inspector"], size: 0.24 }
    ] },
    "Inspect": { column: [
        sized({ row: [
            { tabs: ["sources"], size: 0.18 },
            centre(0.72),
            { tabs: ["inspector", "analysis"], current: "analysis", size: 0.25 }
        ] }, 0.72),
        { tabs: ["scopes"] }
    ] },
    "Edit": { row: [
        { tabs: ["sources"], size: 0.15 },
        centre(0.74),
        sized({ column: [{ tabs: ["inspector"], size: 0.55 }, { tabs: ["metadata"] }] }, 0.24)
    ] },
    "Master": { column: [
        sized({ row: [
            { tabs: ["sources"], size: 0.15 },
            centre(0.5, 0.1),
            { tabs: ["inspector"], size: 0.22 }
        ] }, 0.7),
        { row: [{ tabs: ["scopes"], size: 0.5 }, { tabs: ["compare"] }] }
    ] },
    "Restore": { column: [
        sized({ row: [
            { tabs: ["sources"], size: 0.16 },
            centre(0.74),
            { tabs: ["inspector"], size: 0.22 }
        ] }, 0.74),
        { tabs: ["compare"] }
    ] },
    "Record & Stream": { column: [
        sized({ row: [
            { tabs: ["record", "sources"], current: "record", size: 0.22 },
            centre(0.68, 0.16),
            { tabs: ["streamout", "inspector"], current: "streamout", size: 0.24 }
        ] }, 0.72),
        { tabs: ["console"] }
    ] },
    "Batch": { column: [
        sized({ row: [
            sized({ column: [{ tabs: ["sources"], size: 0.55 }, { tabs: ["presets"] }] }, 0.26),
            centre(0.62),
            { tabs: ["inspector"], size: 0.22 }
        ] }, 0.68),
        { tabs: ["queue"] }
    ] }
};

var views = {
    "Default": {},
    "Convert": { layers: { spectrogram: false } },
    "Inspect": { layers: { loudness: true, phase: true, dc: true } },
    "Edit": { layers: { transcript: true, grid: true } },
    "Master": { layers: { loudness: true }, chainGraph: true },
    "Restore": { spectrogramShare: 0.75 },
    "Record & Stream": { layers: { spectrogram: false } },
    "Batch": { layers: { spectrogram: false } }
};

// A fresh copy of the workspace's layout, ready for DockArea.restoreLayout().
function layout(name) {
    return { version: 1, main: JSON.parse(JSON.stringify(layouts[name])), windows: [] };
}

function view(name) {
    return JSON.parse(JSON.stringify(views[name] || {}));
}

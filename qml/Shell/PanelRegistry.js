.pragma library

// Every dock panel ffgui has, in menu order. DockShell declares one DockPanel
// per entry and the Panels menu lists them from here, so neither reads
// Quilt's internals.
//
//   id             stable: saved layouts and workspaces refer to it
//   area           default zone: center, left, right, bottom (a hint; the
//                  workspace layouts usually decide)
//   core           one of the always-present regions: can't be closed
//   movable, tabbable, floatable   Quilt restrictions (default true)
//   minimumWidth/Height, maximumWidth   content size limits
//   description    for the command palette and tooltips
//   keywords       extra words the command palette matches

var panels = [
    { id: "sources", title: "1 · Sources", area: "left", areaSize: 0.2, core: true, minimumWidth: 230,
      description: "Inputs and their tracks, with the probe summary", keywords: ["inputs", "files", "load", "ffprobe"] },
    { id: "media", title: "2 · Media View", area: "center", core: true, movable: false, tabbable: false,
      minimumWidth: 420, minimumHeight: 230,
      description: "Waveform, spectrogram and analysis layers on one timeline", keywords: ["waveform", "spectrogram", "timeline"] },
    { id: "meters", title: "Meters", area: "right", core: true, tabbable: false,
      minimumWidth: 76, maximumWidth: 220, minimumHeight: 230,
      description: "Live levels, loudness and correlation", keywords: ["levels", "lufs", "peak", "vu"] },
    { id: "chain", title: "3 · Chain", area: "bottom", core: true, tabbable: false, minimumHeight: 118,
      description: "The processing steps, left to right", keywords: ["filters", "effects", "steps", "shape"] },
    { id: "inspector", title: "Inspector", area: "right", core: true, minimumWidth: 270,
      description: "Properties of whatever is selected", keywords: ["properties", "details"] },

    { id: "analysis", title: "Analysis", area: "right", minimumWidth: 260,
      description: "Levels, loudness, dynamics, spectrum, silence and decode errors", keywords: ["astats", "ebur128", "silencedetect", "report"] },
    { id: "scopes", title: "Scopes", area: "bottom", minimumHeight: 140,
      description: "Spectrum, vectorscope, phase, histogram and bit scope", keywords: ["spectrum", "vectorscope", "phase", "histogram"] },
    { id: "metadata", title: "Metadata", area: "right", minimumWidth: 260,
      description: "Tags, cover art and chapters", keywords: ["tags", "id3", "cover", "chapters"] },
    { id: "routing", title: "Routing", area: "bottom", minimumHeight: 140,
      description: "Route channels and build downmix matrices", keywords: ["pan", "channels", "matrix", "downmix"] },
    { id: "compare", title: "Compare", area: "bottom", minimumHeight: 140,
      description: "Original vs processed, or two inputs, side by side", keywords: ["a/b", "difference", "sdr", "psnr"] },
    { id: "console", title: "Console", area: "bottom", minimumHeight: 120,
      description: "The full command, the log and the filter graph", keywords: ["log", "command", "graph", "ffmpeg"] },
    { id: "queue", title: "Queue", area: "bottom", minimumHeight: 120,
      description: "Batch render jobs", keywords: ["batch", "jobs", "render"] },
    { id: "presets", title: "Presets", area: "left", minimumWidth: 200,
      description: "Output and chain presets, built-in and yours", keywords: ["formats", "library"] },
    { id: "record", title: "Record", area: "left", minimumWidth: 230,
      description: "Record from a device as a new input", keywords: ["microphone", "capture", "pulse", "alsa"] },
    { id: "generate", title: "Generate", area: "left", minimumWidth: 230,
      description: "Tones, silence, noise and formulas as new inputs", keywords: ["sine", "noise", "lavfi", "tone"] },
    { id: "streamout", title: "Stream out", area: "right", minimumWidth: 260,
      description: "Live destinations: RTMP, Icecast, SRT, HLS", keywords: ["live", "rtmp", "icecast", "srt", "hls"] },
    { id: "transcript", title: "Transcript", area: "right", minimumWidth: 260,
      description: "Transcript synced to the playhead", keywords: ["whisper", "subtitles", "srt", "speech"] },
    { id: "capabilities", title: "Capabilities", area: "right", minimumWidth: 300,
      description: "What this FFmpeg build supports", keywords: ["build", "codecs", "libraries", "version"] }
];

var ids = panels.map(function (p) { return p.id; });

function panel(id) {
    for (var i = 0; i < panels.length; ++i)
        if (panels[i].id === id)
            return panels[i];
    return null;
}

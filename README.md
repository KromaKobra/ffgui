# ffgui

A panel-based audio workstation for ffmpeg. Signal flows left to right
through four numbered stages (Load, Inspect, Shape, Deliver), with a large
Media View in the centre that shows the audio instead of making you imagine
it. Every edit is a recipe applied at render time; source files are never
touched, and the exact ffmpeg command is always one glance away.

C++ / Qt 6.12 backend driving the installed `ffmpeg`, `ffprobe` and `ffplay`;
a modular QML front end docked with [Quilt](third_party/QUILT_VERSION).

The design lives in the deVault notes: *ffgui Design Concept — Audio* and
*FFmpeg Audio Capabilities — ffgui Reference*.

## Status

Phase 1 (Load and Inspect) works: add files, folders, URLs or drop them in;
ffprobe fills the Sources cards (tracks, chapters, cover art, a health dot from
probing and a full decode check); the Media View draws the real waveform and
spectrogram with zoom down to single samples; one ffmpeg analysis pass gives
loudness, true peak, silence, phase, spectrum, clipping and DC layers, the
Analysis panel and selection stats; playback (original audio) drives live
meters, and Open in ffplay plays from the playhead.

Phase 2 (Shape and Deliver) adds capability-gated steps and formats, filter,
encoder and muxer options from the installed binary's help, an undoable chain
with drag reordering and time ranges, processed waveforms and synchronized
original/processed playback. Live filter options update through ffmpeg's stdin;
other edits restart only the processed decoder. Output presets, naming, signal
format, command preview and negotiated converters drive real renders, selection
exports and a batch queue with pause, retry and reordering. Projects save as
`.ffgui` recipes; user output and chain presets can be saved, imported and exported.

Phase 3 adds editable tags, cover art, chapters and track metadata; an executable
Console; aligned comparison with scores and PCM checksums; six audio scopes;
channel routing; capture devices and recording; generated sources; independent
stream destinations; transcript editing/import/export and a gated Whisper runner;
and FFmpeg tool settings. Metadata edits use the output's Edit policy.

Phase 4 adds graph mode: the Chain as a node editor with extra input tracks as
sources and Mix, Duck, Crossfade, Merge channels, Split, Split channels,
Crossover and Convolution nodes, compiled to a labelled-pad `-filter_complex`.
Graphs drive renders (several outputs on separate branches), preview, playback,
Compare and streaming; tracks dragged from Sources become sources, and Row mode
returns once the graph is a single path again.

Playback has been tested silently; a listening check on your audio device is
still pending. Recording tests use synthetic inputs, streaming tests use local
HLS, and Whisper transcription requires a build with that filter plus a local
whisper.cpp model.

## Build and run

You need Qt 6.12 (Quick, Quick Controls, Multimedia, Quick Test), CMake 3.24+,
Ninja, and ffmpeg/ffprobe/ffplay on the PATH.

```sh
cmake --preset dev            # Debug, build/dev, compile_commands.json
cmake --build --preset dev
build/dev/src/app/ffgui
```

`cmake --preset release` gives an optimized build in `build/release`.

ffgui finds `ffmpeg`, `ffprobe` and `ffplay` on the PATH. To use other
binaries, set `FFGUI_FFMPEG`, `FFGUI_FFPROBE` and `FFGUI_FFPLAY`, or the
`tools/ffmpeg`, `tools/ffprobe`, `tools/ffplay` settings (in
`~/.config/ffgui/ffgui.conf`). What the build can do is cached in
`~/.cache/ffgui/capabilities.json` until the binary changes.

## Test

```sh
scripts/run-tests.sh          # build, import boundaries, qmllint, ctest (offscreen)
scripts/screenshots.sh        # every workspace, ffgui and the Quilt mock-up, in build/screenshots
```

`run-tests.sh` fails on any qmllint warning. Tests run offscreen with a
throwaway settings directory, so they never touch your saved layouts, and
inside a systemd scope capped at 3 GB and two cores (`FFGUI_TEST_MEMORY`
changes the limit). The `ffmpeg`-labelled tests generate small files and use
the real tools; playback tests run at zero volume. `ctest --preset dev -LE
ffmpeg` leaves them out.

## Layout

```
src/core/          ffgui_core: pure C++. tools/ (running ffmpeg), caps/, probe/,
                   media/ (decoding, peak pyramid, spectrogram), analysis/,
                   dsp/ (loudness and scopes), playback/, metadata/, catalog/, log/,
                   capture/, compare/, transcript/,
                   project/ (recipes and undo snapshots), command/, render/
src/backend/       FFgui.Backend: the App singleton, the Engine that runs the
                   jobs, and the models and scene items QML binds to
src/app/           main.cpp
qml/Theme/         colour tokens, metrics, glyphs
qml/Controls/      the Ff* input facade and shared patterns (no Backend)
qml/Shell/         the window, the dock (the only Quilt user), workspaces
qml/Sources, Media, Meters, Chain, Inspector, Output, Tools/<panel>/
                   feature modules, one per region or panel
third_party/Quilt  vendored copy of Quilt (scripts/sync-quilt.sh), never edited
tests/core         unit tests; parsers run on output captured from ffmpeg 9.0.2
tests/integration  the real ffmpeg/ffprobe/audio output on generated files
tests/qml          QuickTest: controls, the shell, load/edit/undo/drag/render
```

`CLAUDE.md` has the layering rules the import check enforces.

## Updating Quilt

```sh
scripts/sync-quilt.sh [path/to/quilt]
```

Copies `Quilt/` from a checkout (default `~/Development/quilt`), records the
commit in `third_party/QUILT_VERSION`, rebuilds and runs the tests. Only
`qml/Shell/DockShell.qml` touches Quilt, through its public API, so that is
where a Quilt change can need a fix.

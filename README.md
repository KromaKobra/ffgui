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

## Build and run

You need Qt 6.12 (Quick, Quick Controls, Multimedia, Quick Test), CMake 3.24+,
Ninja, and ffmpeg/ffprobe/ffplay on the PATH.

```sh
cmake --preset dev            # Debug, build/dev, compile_commands.json
cmake --build --preset dev
build/dev/src/app/ffgui
```

`cmake --preset release` gives an optimized build in `build/release`.

## Test

```sh
scripts/run-tests.sh          # build, import boundaries, qmllint, ctest (offscreen)
scripts/screenshots.sh        # every workspace, ffgui and the Quilt mock-up, in build/screenshots
```

`run-tests.sh` fails on any qmllint warning. Tests run offscreen with a
throwaway settings directory, so they never touch your saved layouts.

## Layout

```
src/core/          ffgui_core: pure C++ (formatting, catalog, and later
                   probing, analysis, DSP, playback, rendering)
src/backend/       FFgui.Backend: the App singleton and the objects QML binds to
src/app/           main.cpp
qml/Theme/         colour tokens, metrics, glyphs
qml/Controls/      the Ff* input facade and shared patterns (no Backend)
qml/Shell/         the window, the dock (the only Quilt user), workspaces
qml/Sources, Media, Meters, Chain, Inspector, Output, Tools/<panel>/
                   feature modules, one per region or panel
third_party/Quilt  vendored copy of Quilt (scripts/sync-quilt.sh), never edited
tests/core, qml    QtTest and QuickTest suites
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

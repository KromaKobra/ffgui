# ffgui handoff: Phases 3–4

Read this before CLAUDE.md's details. It says where ffgui stands, what each
remaining phase delivers, which code is already waiting for it, and how a
phase is judged done.

The approved plan this follows is `~/.claude/plans/review-the-files-in-delegated-liskov.md`
(architecture, key mechanisms, verification). The design is in
`~/Development/deVault/Projects/FFgui/` (*ffgui Design Concept — Audio* and
*FFmpeg Audio Capabilities — ffgui Reference*). The original simulated UI is
`~/Development/quilt/examples/ffgui/` (`Session.qml` holds the mock-up's step
types, command assembly, converter and copy/re-encode logic, useful as a
behavioural reference; leave the folder untouched).

## Working agreement with the user

- **One phase at a time, with a check-in after each**: lint clean, all tests
  passing, offscreen screenshots (`scripts/screenshots.sh`), then a short
  report. Don't start the next phase without the user's go-ahead.
- **Never run anything heavy uncapped.** An uncapped test once filled the
  user's 32 GB of RAM. `scripts/run-tests.sh` and `scripts/screenshots.sh` cap
  themselves; anything run by hand gets
  `systemd-run --user --scope --quiet -p MemoryMax=1500M -p MemorySwapMax=0 -p CPUQuota=100% <cmd>`.
  Generated test media stays seconds long.
- **Ask before opening windows on their desktop or playing audio aloud.**
  Playback tests run at zero volume.
- **Commits only when asked.** Phases 0 and 1 are committed on `main` at
  `c6f05b5`; Phase 2 is uncommitted.
- **Quilt is never edited** (`third_party/Quilt`, synced by
  `scripts/sync-quilt.sh`). Only `qml/Shell/DockShell.qml` touches it, through
  its public API.

## Where things stand

**Phase 0 (scaffold and modular port): done.** CMake presets, one QML module
per region (`qml/Theme`, `Controls`, `Shell`, `Sources`, `Media`, `Meters`,
`Chain`, `Inspector`, `Output`, `Tools/<13 panels>`), the `Ff*` control facade,
the Quilt dock with 8 workspaces, and the full QML-facing API in
`src/backend`. Layering is enforced by `scripts/check-imports.sh`.

**Phase 1 (Load and Inspect): done.**
- Inputs: files, folders, URLs, drag and drop; ffprobe → Sources cards (tracks,
  chapters, tags, cover art via `image://cover`), health from probe + decode check.
- Media View: real waveform and spectrogram (`WaveformItem`, `SpectrogramItem`,
  drawn to textures), overview, zoom to single samples (`WindowDecoder`),
  Shift+scroll amplitude/frequency zoom, log/linear frequency, ruler units,
  chapters, selection, hover readouts.
- Analysis: one ffmpeg pass (`AnalysisPass`: ebur128, astats, silencedetect,
  aphasemeter, aspectralstats) → layers (clipping, silence, loudness curve,
  phase lane, DC), Analysis panel, JSON/CSV export, selection stats.
- Playback of the original (`PlaybackEngine`, Qt's real-time callback API, lock-free
  ring), solo/volume/speed/loop, live meters from a tap (`LoudnessMeter`:
  BS.1770 K-weighting, gating, 4× true peak, within ±0.1 LU of ebur128),
  values at the playhead when stopped, Open in ffplay.
- Capabilities detection (cached), status-bar badge, Capabilities panel,
  Console log of every command.

**Still open from Phase 1:** the user hasn't listened to playback yet (only
silent automated tests). Fix whatever they report first.

**Phase 2 (Shape and Deliver): implemented.**
- `HelpParser` and asynchronous `HelpCache` parse filter/encoder/muxer help,
  including enum choices and runtime flags. Help is cached by binary path,
  size and mtime. The catalog gates steps, codecs, containers and presets;
  user catalog JSON merges from `AppDataLocation/catalog`. The codec and
  container lists now include the reference's common interchange, speech,
  lossless and raw formats, gated by the installed tools.
- Project edits use snapshot undo commands (slider changes merge within
  500 ms). Chain edits, bypass, ranges, row dragging, output configuration,
  selection actions and `.ffgui` New/Open/Save work. Recipe nodes have stable
  ids and explicit links; graph editing remains Phase 4.
- `command/CommandBuilder` contains StepBuilders, ChainCompiler, CopyAdvisor
  and SizeEstimator. It builds argument lists and quoted display groups,
  `-af` for a single output and labelled `asplit` graphs for multiple outputs.
  Output paths must differ from the source and each other. New renders use
  `-n`; retry overwrites only outputs created by that same queue task. Cover
  art is copied for MP3, FLAC and MP4-family outputs; strip omits it.
- `GraphProbe` parses actual negotiated graphs and converter formats. Its
  captured fixture includes the duplicate `sample_rate` key; the parser
  renames the time-base occurrence before JSON parsing. Connectors, the ghost
  converter and Console graph rows use these results.
- `App.media.processed` has its own decoded cache. The Media View overlays
  processed audio over a faint original; the last active step gets a clipping
  badge from processed peaks. Normalize sets the meter target.
- Playback uses two decoders and rings consumed in lock-step. A/B switches
  samples without restarting. Non-runtime edits restart only the processed
  decoder after 300 ms; runtime options send stdin commands. Input pacing
  (`-readrate`, with a small initial burst and loudnorm lookahead) prevents
  FFmpeg from buffering seconds beyond the playhead and delaying live edits.
  Timeline steps stay out of preview; trim sets the play range and speed
  disables aligned A/B.
- `render/RenderQueue` contains RenderJob, RenderQueue and BatchExpander.
  Direct renders, selection exports, graceful stop, queued batches, pause,
  retry and reorder work. Batch files are probed serially so each uses its
  own duration and signal format. Queue pause lets running jobs finish.
- User output/chain presets persist under `AppDataLocation/presets`, with
  import/export dialogs. Presets preserve explicit options, bypass and ranges.

**Next: Phase 3, after the user's go-ahead.** Do not start it automatically.
The remaining `isn't available yet` stubs are Phase 3/4 features (metadata,
Console execution, generated sources, cover replacement and impulse responses).
The user still needs to listen to playback; fix anything they report first.

**Tests:** 13 ctest suites: `tests/core` (unit; parsers run against output
captured from this machine's ffmpeg 9.0.2 in `tests/core/fixtures`),
`tests/integration/tst_pipeline` (label `ffmpeg`: real tools on generated
files, including zero-volume playback), `tests/core/tst_delivery` (help, recipes, snapshot undo, command goldens and
GraphProbe), `tests/integration/tst_delivery_pipeline` (all built-in output
and chain presets, multi-output, loudness, stopped-file validity, queue
retry/actions, selection export, 24-bit output, silent A/B/live changes,
processed decoding and project reopen), `tests/qml` (controls; the shell:
every panel opens, closes and pops out, every workspace applies, layouts
round-trip; `tst_load.qml`: loads a generated file, inserts a step through the picker,
undoes/redoes and drags it, and renders through the real window).

Verification: `scripts/run-tests.sh` (build, import boundaries, zero qmllint
warnings, all 13 suites); `scripts/screenshots.sh` captures all eight
workspaces with a real chain and output in `build/screenshots/`.

## How the code is organised (what you'll extend)

| Where | What |
|---|---|
| `src/core/` | Pure C++, no QML. `tools/ProcessJob` runs every ffmpeg (arg list, `-loglevel level+...` stderr parsing, graceful `q` stop, `setBackground`, logging). `caps/` build info. `probe/`. `media/` (`PcmReader`, `PeakPyramid`, `Spectrogram`, `MediaData`). `analysis/`. `dsp/`. `playback/`. `catalog/` (JSON in `catalog/data`). `log/LogStore`. |
| `src/backend/` | `App` (QML singleton; owns every sub-object), `Engine` (runs the jobs for the input on screen and wires Transport/Meters/Analysis), `Project` (inputs, chain, outputs, undo stack), `MediaState` (`App.media`), list models (`Models.h/.cpp`, roles are the QML contract), scene items (`SceneItems.cpp`). |
| `qml/` | Already binds to the Phase 2–3 API. Most remaining work is filling C++ so the existing QML comes alive, not writing new QML. |

Stubs that toast "isn't available yet" mark unbuilt features:
`grep -rn "available yet" src qml`.

---

## Phase 2: Shape and Deliver (implemented; original checklist)

This section is retained as implementation context. The next work is Phase 3.

Goal: the chain edits the sound, outputs are configured, the exact command is
built, the processed result can be seen and heard, and renders run.

### 2.1 Catalog gating and full option lists
- Gate `App.catalog.steps` (`StepCatalogModel` roles `available`, `reason`)
  against `App.caps` (filters each step `requires`): unavailable steps say
  "Needs librubberband, which this build lacks". Same for encoders
  (`-encoders`) and containers (`-muxers`) in the Output bar.
- New `src/core/caps/HelpParser`: parse `ffmpeg -h filter=X`, `-h encoder=X`,
  `-h muxer=X` into `ffgui::OptionSpec` (name, type, min, max, default, enum
  choices, **runtime flag** = `T` in the flags column such as `..F.A....T.`).
  Cache on disk like capabilities. Fill `CatalogFacade::m_help`;
  `filterOptions()`, `encoderOptions()` and `muxerOptions()` already serve it to QML.
  `StepSheet.qml` already shows "Advanced" = curated advanced options plus
  every other `-h` option. `OutputSheet.qml` lists encoder/muxer options
  read-only; make them editable with `ParamControl`.
- User catalog directory (`QStandardPaths::AppDataLocation`/`catalog`) merged
  over the built-in one (`Catalog::merge` exists and is tested).

### 2.2 Project model with undo
- New `src/core/project/`: recipe types: inputs (file, folder, url, device,
  lavfi generator), chain graph (nodes with `uid`, `type`, `params`, `bypass`,
  `range`; links; a row is a graph with one path), outputs, metadata edits,
  markers. `RecipeJson` load/save for `.ffgui` files (round-trip tested).
- Every mutation in `Project` goes through `QUndoStack` with snapshot commands;
  slider drags merge by key + time (`QUndoCommand::mergeWith`). `App.undo`
  already reflects the stack; header buttons and Ctrl+Z/Ctrl+Shift+Z are wired.
- Implement the stubbed `Project` methods: `insertStep`, `addStep`,
  `removeStep`, `moveStep`, `setBypass`, `setParam`, `setRange`, `clearRange`,
  `applyChainPreset`, `applySelectionAction` (trim, cut, fadeIn, fadeOut,
  silence, normalize, step, export, marker), `addOutput`, `removeOutput`,
  `setOutput`, `applyPreset`, `newProject`, `open`, `save`. Keep `Selection`
  pointing at the right item (`Selection::itemInserted/Removed/Moved` exist).
- Fill `StepsModel` rows. QML already binds to: `uid, type, name, glyph,
  category, filter, bypass, params, hasRange, rangeStart, rangeEnd,
  rangeSupported, timeline, available, warning, formatIn, formatOut,
  formatChanged, filterText`. Fill `OutputsModel` rows: `uid, pattern,
  fileName, folder, preset, container, codec, codecLabel, quality, rate, depth,
  channels, metadata, kbps, sizeBytes, sizeText, reencodeReason, lossyToLossy,
  problem`. Fill `Project::converter` (`{ filter, from, to, reason }`),
  `keptStart/keptEnd` (from a Trim step) and `totalSizeText`.
- Chain row view: drag to reorder (QML side, `ChainRowView.qml`). The + picker
  and the range bands in the Media View (`OverlayBand` → `setRange`) are
  already wired.

### 2.3 Command building (`src/core/command/`)
- `StepBuilders`: the default template builder (fills `{key}` from params;
  numbers trimmed, booleans 1/0) plus the named builders the catalog
  references (`builder` field in `steps.json`): `trim` (atrim+asetpts, or
  `-ss/-to` when stream-copying), `eq` (two filters), `dehum` (bandreject at
  the mains frequency and `harmonics` multiples), `pitch` (param `semitones` →
  `rubberband=pitch=2^(st/12)`), `mono` (pan formula from the channel count),
  `swap`, `mix` (param `matrix` is a pan spec from Routing), `transcribe`.
  Name every instance `filter@<uid>` so live commands can address it.
- Time ranges: `enable='between(t,a,b)'` only for filters with the T flag in
  `-filters` (`BuildInfo::filters[name].timeline`); otherwise
  `rangeSupported=false` and the sheet says so (`StepSheet.qml` already does).
- `ChainCompiler`: `-af` for one output; `-filter_complex` with `asplit` for
  several (`[a0][a1]` per output, `-map`).
- `CommandBuilder`: args + shell-quoted display groups; feed `App.command`
  through `Command::set(args, groups)`. Remove the placeholder logic in
  `Engine::updateBlockers` once a real command exists, and clear
  `App.render.blocker` when rendering can run.
- `CopyAdvisor`: stream copy vs re-encode with the reason; lossy → lossy
  warning; "Sample-exact" (atrim) vs "Snaps to nearest packet" (copy + -ss/-to).
  `SizeEstimator`: `CodecSpec::estimateKbps` exists; × kept duration.
- Golden argument-list tests for builders, compiler and command (plan:
  Verification).

### 2.4 Negotiated graph (`GraphProbe`)
- A debounced dry run: `ffmpeg -v verbose … -t 0.05 -f null - -print_graphs
  -print_graphs_format json`. The JSON gives topology, auto-inserted
  `auto_aresample_N` and per-link rate/layout. It has a **duplicate
  `sample_rate` key** (the second is the time base), so parse tolerantly. The
  verbose lines `auto_aresample_0 … fmt:s16 r:44100Hz -> … fmt:fltp
  r:44100Hz` give sample formats. (Both per the plan; recapture fixtures
  from this build before writing the parser.)
- Drives `formatIn/formatOut/formatChanged` on steps, `Project::converter` (the
  dashed ghost block in `ChainRowView`) and `App.project.graph` rows (the
  Console's Filter graph tab, `GraphTab.qml`, shows `kind == "auto"` rows in
  amber).

### 2.5 Processed preview and A/B
- Processed decode: the same `PcmReader`/`MediaData` path with the compiled
  chain (timeline steps left out: trim becomes the play range, pad/speed
  excluded; speed disables A/B alignment with a reason in
  `Transport::setAbAvailability`). Cache by input + chain hash (`MediaState`
  keeps one cache today; give it a second, processed slot).
- Media View: when `layers.processed` is on and the chain has active steps,
  draw the processed waveform over a faint ghost of the original
  (`Theme.waveGhost` exists). Clip badges: `StepsModel.warning` ("clips +2.1 dB")
  from processed peaks.
- Playback A/B: `PlaybackEngine` gets two decoders (original, processed) in
  lock-step and switches with `Transport.ab` without a restart.
- Live parameters: options flagged runtime are sent to the processed
  decoder's stdin as `c` + `volume@s3 -1 volume 0.5\n` (plan: verified to change
  the level mid-stream). Anything else restarts the processed decoder at the
  playhead after 300 ms. `ParamControl` already labels runtime options
  "(applies live)".
- `Meters.setTarget()` from a Normalize step (`I`), so the meters and loudness
  layer show the target line.

### 2.6 Rendering, queue, presets, projects
- `src/core/render/`: `RenderJob` (progress from `-progress pipe:1` via
  `ProgressParser`, speed, ETA, `ProcessJob::stop()` sends `q` so files stay
  valid), `RenderQueue` (concurrency, pause, retry, reorder), `BatchExpander`
  (one job per file × output; folder inputs keep their file list in
  `Project::InputInfo::files`).
- Handle `Render::startRequested(scope)` (`all` / `selection`),
  `stopRequested`, `enqueueRequested`, and `QueueModel::actionRequested(action,
  row)`. Toast on finish with `open:<path>` / `reveal:<path>` actions
  (`App::runAction` exists).
- Presets panel: user presets (save current, import, export) in
  `AppDataLocation`; the dialog ids `savePreset`, `importPresets`,
  `exportPresets` fall through to "isn't available yet" in
  `qml/Shell/FileDialogs.qml` today.
- Project New/Open/Save (`.ffgui`, dialogs wired).
- Quick-convert empty state (`QuickConvert.qml`) and the Output bar's "Choose a
  format" row already call `addOutput(preset)` and `App.render.start()`.

### Phase 2 is done when
- Unit: golden args for StepBuilders/ChainCompiler/CommandBuilder, CopyAdvisor,
  SizeEstimator, HelpParser and GraphProbe on captured fixtures, Recipe
  round-trip, undo/redo (including merged drags).
- Integration (label `ffmpeg`): each built-in preset renders and ffprobe
  confirms codec, rate and channels; a loudnorm −16 render measures −16 ±1 LU;
  stopping a render with `q` leaves a valid file; a live command changes the
  measured level; GraphProbe reports the inserted converters.
- QML: build a chain through the UI, undo it, render a short fixture, all
  without warnings; screenshots show the chain, connectors and output bar.

---

## Phase 3: toggleable panels and transient UI

The QML for every panel exists (`qml/Tools/*`) with empty states; most need a
backend.

- **Metadata**: tags (common form + custom keys; `Project::setTag/removeTag`
  are stubs, `Project::tags` exists), cover extract/replace/remove
  (`CoverArt` extracts already; replace = map a new attached picture on
  output; dialog ids `extractCover`, `replaceCover`), chapters ↔ Media View
  markers (editable; write with an ffmetadata file), per-track title,
  language and default flags. Used by outputs whose policy is "edit".
- **Console**: run the edited command as-is (`App::runCommand` stub; split the
  text into args without a shell, log the output in the Log tab). Edits stay
  one-way (never parsed back into the chain).
- **Compare**: original vs processed, or two inputs: aligned waveforms, a
  difference trace, offset finder (envelope cross-correlation), `asdr`/`apsnr`
  scores, md5 checksum match. Needs a backend object (`App.compare`).
- **Scopes**: six kinds (spectrum bars, constant-Q, vectorscope, phase,
  level histogram, bit scope), tiled or single, fed from the playback tap
  (`PlaybackEngine::takeTap`; today only the meters drain it, so share it).
  New `dsp/` classes: SpectrumAnalyzer, Histogram, BitScope, Vectorscope;
  draw them as textures like `SceneItems.cpp`.
- **Routing**: the matrix UI exists and calls `addStep("mix", { matrix })`,
  which works once Phase 2 lands.
- **Record**: `DeviceList` (`ffmpeg -sources pulse`, alsa) into
  `App.devices` (roles `id, name, backend, isDefault, isMonitor`); `Recorder`
  (pulse input → file, an f32le monitor pipe for level and elapsed time,
  `-t`/`-fs` limits, `-f segment` splitting); the result becomes an input.
- **Generate**: `Project::addGenerator(lavfiSource, name)` stub. The panel
  builds the lavfi source; render it to a temp file (or add a lavfi input
  kind), then probe it like any input.
- **Stream out**: one process per destination (RTMP, Icecast, SRT, HLS) into
  `App.destinations` (roles `uid, name, url, protocol, format, status,
  bitrate`), bitrate history from progress output, connection lights.
- **Transcript**: gated on the `whisper` filter (missing in this build);
  SRT/TXT/JSON import and export into `App.transcript` (`start, end, text`) so
  the editor and the Media View's transcript lane work without whisper.
  Dialog ids `importTranscript`, `exportTranscript`.
- **Command palette** (exists; `ActionRegistry.qml`): add actions as features land.
- **Drop overlay**: "Use as cover art" (`setCoverArt` stub), "Use as impulse
  response" (`useImpulseResponse` stub: an `afir` convolution step; it needs a
  second input, so it may belong in Phase 4).
- Remaining empty states; a small Settings surface for tool paths is worth
  adding (`tools/ffmpeg` etc. settings are already honoured by `FfTools`).

Done when each panel works end to end on generated fixtures with tests per
backend object, panels open/close/pop out cleanly (the existing shell test
covers that), and screenshots of each workspace look right with data.

---

## Phase 4: graph mode

- A node editor over the chain graph (`ChainGraphView.qml` is today a
  read-only one-path view; `App.project.graph` is a `GraphModel` with roles
  `uid, kind, label, detail, x, y, inputs, outputs, bypass`; links need adding).
- Multi-input and multi-output nodes: Mix (`amix`), Duck
  (`sidechaincompress`), Crossfade (`acrossfade`), Merge (`amerge`/`join`),
  Split (`asplit`, `channelsplit`, `acrossover`), and extra inputs as source
  nodes (the "Drop a track here to mix it in" target exists).
- Compile to a labelled-pad `-filter_complex`.
- Switching back to row mode is disabled only while the graph has branches.
  The Master workspace opens in graph mode (`Workspaces.js` view
  `chainGraph: true`).

---

## Gotchas already hit (don't repeat them)

- QML Repeater counts derived from separately updating bindings can briefly
  ask for millions of delegates. Build the list in one binding and cap it.
  Item-per-entry overlays show only what's visible.
- `Q_INVOKABLE` calls inside bindings don't re-evaluate when data changes;
  bind to NOTIFY properties.
- The offscreen/software scene graph draws no custom geometry nodes; draw to
  images (see `SceneItems.cpp`).
- `App` must stay non-default-constructible so QML uses `App::create()`
  (which registers `image://cover`).
- Backend type names must not clash with QML file names in feature modules
  (`Transport` vs `TransportBar.qml`). Model roles must not reuse Item
  property names (`state`, `text` on a Text). Ids must not shadow
  properties of their own type (`palette`, `menu`, `scale`, `dock`).
- `qsTr("%n …")` doesn't pluralise without translation files; use explicit
  singular/plural strings.
- `QHash` values must be copyable (use `shared_ptr`, not `unique_ptr`);
  `requires` is a C++20 keyword.
- Hot DSP files are compiled `-O2` even in Debug (`src/core/CMakeLists.txt`);
  add new per-sample code there.
- ffmpeg's "Guessed Channel Layout" warning is benign (`LogLine::benign`).

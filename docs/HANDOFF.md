# ffgui handoff: Phase 4 next

Read this before `CLAUDE.md`. Phases 0–3 are implemented. The next implementation
phase is **Phase 4: graph mode**, after the user's go-ahead. Do not start it
automatically. Phase 3 changes are uncommitted; Phase 2 is committed on `main`
at `14985b3` (`stage 2 complete`). Do not discard the working tree.

The approved plan is `~/.claude/plans/review-the-files-in-delegated-liskov.md`.
Design references are in `~/Development/deVault/Projects/FFgui/`:
*ffgui Design Concept — Audio* and *FFmpeg Audio Capabilities — ffgui Reference*.
The simulated UI in `~/Development/quilt/examples/ffgui/` is a reference only;
leave it untouched. Its `Session.qml` is useful for intended interactions.

## Working agreement

- One phase at a time, followed by clean lint, passing tests, offscreen
  screenshots, and a short report. Wait for the user before the next phase.
- **Cap every heavy command**, including builds and ad hoc FFmpeg runs:
  `systemd-run --user --scope --quiet -p MemoryMax=1500M -p MemorySwapMax=0 -p CPUQuota=100% <command>`.
  An uncapped test once exhausted the user's 32 GB. Use at most two build
  jobs (`CMAKE_BUILD_PARALLEL_LEVEL=2`). Keep test media seconds long.
  The scripts also apply internal limits; an outer scope keeps their builds
  capped too. Set `FFGUI_TEST_MEMORY=1500M` for `run-tests.sh`.
- Ask before opening desktop windows or playing audible audio. All automated
  playback runs at zero volume. Capture tests use lavfi, never the microphone;
  streaming tests use local HLS, never a remote destination.
- Commit only when asked.
- Never edit `third_party/Quilt`. Only `qml/Shell/DockShell.qml` uses its public
  docking API. Preserve feature-module import boundaries and Ff control wrappers.

## Implemented phases

**Phase 0:** modular CMake/QML scaffold, the Ff control facade, Quilt docking,
13 tool panels and eight workspaces. `scripts/check-imports.sh` enforces layering.

**Phase 1:** files/folders/URLs/drop, ffprobe tracks/tags/chapters/cover art,
waveform/spectrogram and sample zoom, analysis layers and exports, original
playback, meters, ffplay, capability detection and command logging.

**Phase 2:** capability-gated catalog and parsed tool help, snapshot undo,
chain editing/ranges/reordering, recipes and presets, command compilation,
negotiated converters, processed decoding and synchronized A/B playback,
live filter options, direct/selection renders and the batch queue. Snapshot
recipes already contain stable node ids and explicit links for a linear graph.

**Phase 3:**

- Metadata: common/custom tags, cover extraction/replacement/removal, editable
  chapters and per-track title/language/default flags. Edits are per input uid,
  undoable, and persist in the optional `edits` field of version-1 `.ffgui`
  recipes. Outputs with metadata policy `edit` use these values; `copy` and
  `strip` retain their policies. FFmetadata and PNG sidecars are content-addressed
  in the cache; `BuiltCommand::prepare()` materializes them for renders and
  copied/Console commands. Attached pictures are supported in MP3, FLAC and
  MP4-family outputs; other containers follow their existing cover support.
  Chapters also drive Media View bands and transport marker navigation. Batch
  enqueue snapshots metadata per source instead of applying the selected input's
  tags to every source.
- Console: edited commands run as FFmpeg argument lists through `ProcessJob`,
  with quoting/continuations, logging, errors and Stop. No shell is invoked;
  operators/pipelines are rejected and edits never mutate the project.
- Compare: original versus preview processing, or two inputs; signed B offset,
  envelope correlation search, aligned peak/difference traces, `asdr`/`apsnr`
  and decoded PCM MD5. Audio normalizes to 48 kHz stereo for scores/checksums;
  scores use the overlap, hashes cover each aligned stream. Display/envelope
  work is bounded to the first 30 seconds at 16 kHz mono, offset search to
  ±2 seconds (also bounded by available overlap). Scores/hash jobs decode
  the full sources. Offset estimates are quantized to 10 ms.
- Scopes: spectrum, constant-Q musical bands, vectorscope, phase, level histogram,
  and 24-bit activity. `Engine::tick()` shares each playback tap chunk between
  scopes and meters. Stopped scopes use their own bounded PCM read at the
  playhead; they must never borrow the Media View's single zoom window.
  Musical analysis caps its longest window at 4096 frames, limiting bass
  resolution. Texture rendering works with the offscreen/software renderer.
- Routing: editable gain matrix adds a working `pan`/Channel mix step.
- Recording: PulseAudio/PipeWire and ALSA discovery, recording format/path,
  input level/elapsed time, duration/size limits, optional segment splitting,
  graceful stop, and recorded files added as inputs. Existing files/segment
  patterns are rejected. The monitor pipe always drains; the level checkbox
  controls measurement, not audible monitoring. Output packet flushing lets
  the size watchdog observe bytes even while the monitor output remains open.
- Generate: tones (with amplitude), silence, noise, expressions, and gated flite
  render asynchronously to persistent `AppDataLocation/generated/*.wav`, then
  become probed inputs. Their path/name/kind/lavfi expression survive save/reopen.
  New/Open cancel in-flight generation. Successful generated files are retained
  so saved recipes and undo/redo can reopen them.
- Stream out: independent RTMP/RTMPS, Icecast MP3, SRT MPEG-TS and local HLS
  processes using the current source/chain snapshot. Each destination has
  status, error, measured bitrate and a bounded 120-reading history. HLS
  measures its uniquely named segment files because FFmpeg reports no
  `total_size` there. Stop/Stop all are graceful; HLS requires a new playlist path.
- Transcript: editable start/end/text, seek on timestamp, add/remove,
  SRT/TXT/JSON import/export and the Media View lane. Untimed TXT lines get
  five seconds each. Documents are retained per input during the session;
  export them to retain edits across application restarts (they are not recipe
  fields). A gated Whisper runner accepts a local whisper.cpp model/language,
  writes SRT in a temporary directory and imports it. Changing inputs cancels it.
  The installed FFmpeg has no Whisper filter, so only its unavailable path is
  exercised here. Its arguments follow the official
  [Whisper filter documentation](https://ffmpeg.org/ffmpeg-filters.html#whisper).
- Shell: palette actions, working cover drop, remaining empty states and tool
  path settings (header menu and palette). Settings validate/resolve executables
  and refresh capabilities. The impulse-response drop target is deferred to
  Phase 4 because `afir` requires a second graph input; the dead target is hidden.

## Verification and limits

Verified on 2026-11-01 against this machine's FFmpeg 9.0.2 and Qt 6.12:

- Resource-capped `scripts/run-tests.sh`: build passes, import boundaries pass,
  **zero qmllint warnings, all 15 ctest suites pass** (35.37 seconds for ctest).
  This includes the existing delivery/playback regressions and panel
  open/close/pop-out/workspace tests.
- New `tests/core/tst_tooling.cpp`: command quoting/rejection, transcript formats
  and validation, captured device/score parsers, capture/stream arguments,
  correlation offsets, scope signal properties, metadata escaping and settings.
- New `tests/integration/tst_tooling_pipeline.cpp`: real metadata/chapter/track
  renders, cover replacement/removal/extraction, undo and recipe reopen,
  identity/gain/delayed comparison and hashes, synthetic recording limits/splits/
  stop/overwrite refusal, HLS bitrate/finalization/stop, backend capture/stream
  state, per-input documents/metadata, Console run/stop, generator and stopped
  scopes. A regression check protects the independent waveform zoom window.
- New `tests/qml/tst_tooling.qml`: real metadata/undo, Compare, all six scope
  modes (including a rendered phase pixel), routing and transcript workflows.
  Its test surface must be visible for the pixel check. Sample pixel positions
  are mapped into the captured root item's coordinates.
- `scripts/screenshots.sh` captures all eight workspaces in `build/screenshots/`
  with a real chain/output, comparison results, transcript, metadata and an idle
  local HLS destination. Images were inspected; scope content sizing and compact
  comparison layout were corrected. Quilt's mock-up is unchanged.
- `git diff --check` is clean. No commits were created.

The user's listening check remains outstanding. Actual microphone/loopback
recording and remote RTMP/Icecast/SRT delivery have not been exercised: their
argument construction is tested, while capture uses synthetic input and streaming
runs local HLS. Whisper inference needs another FFmpeg build and a model; it is
capability-gated, not simulated. Do not describe these as manually verified.

## Code map and contracts

| Location | Responsibility |
|---|---|
| `src/core/tools/` | `ProcessJob` for every tool invocation, progress/log parsing, paths, quoting, and the Console tokenizer. |
| `src/core/project/` | Recipe JSON and snapshot undo. Version 1 accepts optional per-input metadata `edits`; old recipes remain readable. |
| `src/core/command/` | StepBuilders, ChainCompiler, CopyAdvisor, SizeEstimator, CommandBuilder and GraphProbe. Today the compiler consumes row nodes and builds one linear chain or output `asplit`. |
| `src/core/render/` | RenderJob, RenderQueue, BatchExpander; `RenderJob` prepares sidecars before starting. |
| `src/core/metadata/` | Cover extraction plus FFmetadata serialization. |
| `src/core/capture/` | DeviceList, Recorder/CaptureSpec, StreamOut/StreamSpec; no QML dependencies. |
| `src/core/compare/`, `transcript/`, `dsp/Scopes.*` | Comparison pipeline/correlation, transcript formats/validation, scope DSP. |
| `src/backend/Project.*` | Inputs/probes, row nodes, outputs, metadata edits, recipe/undo mutations and generated-source job. `ProjectEdit` coalesces nested mutations. |
| `src/backend/Engine.*` | Current-input analysis/decoding/playback, compiler/render queue wiring, tap sharing, stopped scope reads. |
| `src/backend/Tools.*` | App-owned ConsoleSession, TranscriptDocument, CompareState, ScopeState, CaptureState, StreamState and ToolSettings. |
| `src/backend/ToolPlot.*` | Scope, waveform envelope and bitrate-history textures. |
| `src/backend/Models.*`, `RowModel.*` | Fixed role lists are the QML contract; use fine-grained `update()`. `App.transcript` is now TranscriptDocument, a RowModel subclass. |
| `qml/` | Feature modules import only Backend/Theme/Controls; Shell owns cross-panel dialogs, actions, drop targets and docking. |

New App properties are `console`, `compare`, `scopes`, `capture`, `stream`,
`settings`; `transcript` now exposes editing/import/export/transcription methods.
Devices retain `id,name,backend,isDefault,isMonitor`; destination roles add
`kbps,history,error` to `uid,name,url,protocol,format,status,bitrate`.

Project exposes `tags`, `chapters`, `editableTracks`, `cover`, `generating`,
`setTag/removeTag`, `setChapter/addChapter/removeChapter`, `setTrackMetadata`,
`setCoverArt/extractCover/removeCover`, and `outputMetadata(inputIndex=-1)`.
Use stable input ids for durable edits; list indices are only a view projection.

## Next: Phase 4 graph mode

The approved remaining work is:

1. Turn `ChainGraphView.qml` into an editable graph over recipe nodes/links.
   It is currently a read-only linear projection; the graph model has
   `uid,kind,label,detail,x,y,inputs,outputs,bypass`, but no editable link model.
2. Represent extra inputs as graph sources and support Mix (`amix`), Duck
   (`sidechaincompress`), Crossfade (`acrossfade`), Merge (`amerge`/`join`),
   Split (`asplit`, `channelsplit`, `acrossover`) and the deferred IR/`afir` source.
3. Compile graph links and labelled pads into `-filter_complex`, preserving
   output configuration/metadata, preview, render and queue behavior. Keep
   single-path recipes and existing linear compilation compatible.
4. Allow row mode whenever the graph has one path; disable switching back only
   while branches require graph mode. Master already opens with `chainGraph: true`.
5. Add generated multi-input integration fixtures, graph/undo/recipe/compiler
   tests and real QML interactions, then run the full lint/tests/screenshots.

Read the original plan and design before choosing graph APIs. Phase 3 did not
add graph operations or multi-input compilation. Source file/listening issues
reported by the user take priority over extending the graph.

## Gotchas to preserve

- QML Repeaters must use a bounded list/count derived in one binding. A transient
  mismatch once created millions of delegates. Transcript imports are capped,
  but very large transcript lanes may still benefit from visible-window projection.
- Invokable calls in bindings need an explicit NOTIFY dependency (for example
  `model.revision` or `App.caps.state`). Use fixed models for live data rather
  than rebuilding delegates every meter tick.
- Custom scene items must draw texture images for the software renderer.
- `App` stays non-default-constructible so QML uses `App::create()` and installs
  the cover image provider.
- Avoid backend/QML type-name collisions, Item property/role collisions, and ids
  such as `palette`, `menu`, `scale`, `dock` that shadow their own type properties.
- Parse captured FFmpeg output, not imagined formats. GraphProbe's fixture has
  a duplicate `sample_rate` key (time base versus numeric rate); its tolerant
  parser handles this deliberately.
- New per-sample DSP needs `-O2` even in Debug (`src/core/CMakeLists.txt`).
- Cancel/disconnect owned jobs before dependent App services are destroyed.
  Capture/stream jobs attempt graceful stop so files finalize; background decode
  and comparison jobs may be killed.
- Do not let tests use real user settings, generated recordings, streams or
  audible playback. Tests/screenshots run with temporary configuration.

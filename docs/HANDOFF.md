# ffgui handoff: Phase 4 implemented, awaiting the user's review

Read this before `CLAUDE.md`. Phases 0–3 are committed on `main` at `09810be`
(`stage 3 complete`). **Phase 4 (graph mode) is implemented and verified in the
working tree, uncommitted**; its new files (`src/core/graph/`,
`tests/core/tst_graph.cpp`, `tests/integration/tst_graph_pipeline.cpp`,
`tests/qml/tst_graph.qml`) are untracked until the user stages them. Commit only
when asked.

The approved plan is `~/.claude/plans/review-the-files-in-delegated-liskov.md`;
it ends at Phase 4. Design references are in `~/Development/deVault/Projects/FFgui/`:
*ffgui Design Concept — Audio* and *FFmpeg Audio Capabilities — ffgui Reference*.
The simulated UI in `~/Development/quilt/examples/ffgui/` is a reference only;
leave it untouched. Its `Session.qml` is useful for intended interactions.

## Working agreement

- One phase at a time, followed by clean lint, passing tests, offscreen
  screenshots, and a short report. Wait for the user before going on.
- If the user warns that credits are nearly exhausted, stop implementation and
  testing immediately and document the exact state and remaining work instead.
- **Cap every heavy command**, including builds and ad hoc FFmpeg runs:
  `systemd-run --user --scope --quiet -p MemoryMax=1500M -p MemorySwapMax=0 -p CPUQuota=100% <command>`.
  An uncapped test once exhausted the user's 32 GB. Use at most two build
  jobs (`CMAKE_BUILD_PARALLEL_LEVEL=2`). Keep test media seconds long.
  The scripts also apply internal limits; an outer scope keeps their builds
  capped too. Set `FFGUI_TEST_MEMORY=1500M` for `run-tests.sh`.
- Ask before opening desktop windows or playing audible audio. All automated
  playback runs at zero volume. Capture tests use lavfi, never the microphone;
  streaming tests use local HLS, never a remote destination.
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
live filter options, direct/selection renders and the batch queue.

**Phase 3:** metadata (tags, cover, chapters, track flags; per input uid, in the
recipe's `edits`), Console, Compare, six scopes, routing matrix, recording,
generated sources, stream destinations, transcripts with a gated Whisper runner,
tool settings. Details and limits are in git history (`09810be`'s HANDOFF).

**Phase 4: graph mode.**

- **Editor** (`qml/Chain/ChainGraphView.qml`, `NodeCard.qml`): a wrapping
  toolbar (+ Source and + Node menus, + Effect…, Remove, the connection list
  with remove, and a hint/error line), draggable cards with labelled ports
  (main/sidechain, first/second, signal/response, channel names, crossover
  bands), hollow unconnected inputs, an error outline for unreadable sources,
  and an empty state. Click an output port, then an input port, to connect
  (a dashed wire follows the pointer); clicking a connected input disconnects
  it; a new connection replaces the input's old one. Selecting a source card
  shows its input in the Media View and Inspector; port clicks never select.
  Tracks dragged from Sources (sideways) into the Chain become a source where
  they land, wired into the card they land on (an output's feed is replaced);
  in Row mode the drop makes that track the chain's input.
- **Operations** (catalog category Graph): Mix (`amix`), Duck
  (`sidechaincompress`), Crossfade (`acrossfade`, overlap and curves), Merge
  channels (`amerge`), Split (`asplit`), Split channels (`channelsplit`), Crossover
  (`acrossover`) and Convolution (`afir`, also the window's impulse-response drop
  zone). Ordinary steps work as one-in/one-out nodes, bypass passes the first input.
- **Compiler** (`src/core/graph/GraphCompiler`): validates ids, ports, single
  connections per input and cycles; orders topologically; inserts `asplit` for
  fan-out and `anullsink` for unused ports; emits a labelled-pad
  `-filter_complex`. Bounds: 256 nodes, 1024 links, 16 sources, 8 ports (24 for
  channel splits). GraphPlan carries input args, filter, output pads, duration,
  `aligned` and `seekable`.
- **Rows and graphs:** a linear project derives its graph; the first wiring,
  source or multi-port node promotes it into the recipe's `graph` field. An
  active graph always shows as a graph; Row is disabled while it branches and
  otherwise turns it back into ordered rows (one undo step).
- **Everywhere downstream:** renders (several outputs on separate branches,
  metadata Copy/Edit/Strip, cover replacement, selection exports), processed
  decoding and playback, the negotiated graph/Console, Compare (graph as B),
  stream destinations (the selected output's branch) and the queue.

## Phase 4 verification (2026-11-01, FFmpeg 9.0.2, Qt 6.12)

- Resource-capped `scripts/run-tests.sh`: build, import boundaries, **zero
  qmllint warnings, all 17 ctest suites pass** (48.8 s). New suites: `tst_graph`
  (core) and `tst_graph_pipeline`; `tst_graph.qml` joins the QML suite.
- `tests/core/tst_graph.cpp`: shape errors, golden filter text for every
  operation, durations (amix modes, crossfade overlap, amerge shortest), option
  pass-through/refusal, bypass, preview sinking, fan-out, channel layouts, port
  names, seekable windows (`PcmReader::decodeArgs`), the linear check and the
  recipe `graph` field (bounds; Phase 3 recipes still load).
- `tests/integration/tst_graph_pipeline.cpp` (real FFmpeg): every operation
  renders and decodes with the right length and channels; Duck repeated without
  flakes; two-output fan-out; seeked windows match full decodes; silent playback
  past a crossfade's first file with a mid-play graph edit, a speed change and a
  return to rows; A/B availability rules; project topology (port-count edits,
  cycles refused, undo/redo exactness, removed inputs, positions pruned, a
  hand-edited recipe with a missing track and duplicates, folders refused);
  the impulse-response action; two branches with Edit tags + new cover vs Strip,
  44.1 kHz mono MP3 vs FLAC, then a selection export; output names/tags from the
  right source; save/reopen, render, local HLS and Compare of a graph.
- `tests/qml/tst_graph.qml`: the real panel's menus, port wiring, disconnect and
  undo, drag and undo (cards survive model updates), source selection, locked
  Row mode, save/reopen, removal back to a row, and the track drop handler.
- `scripts/screenshots.sh`: all eight workspaces plus `ffgui-master-graph.png`
  (a voice chain ducking a music bed, then mixed) were inspected; a 420 px
  Chain panel was checked separately for toolbar wrapping. Quilt's mock-up is
  unchanged. `git diff --check` is clean.

**Not verified by hand:** listening (all playback ran at zero volume), a native
pointer drag from Sources (offscreen windows have no drag and drop; the drop
handler is tested directly), real capture devices and remote stream targets,
and Whisper (Phase 3 gaps). Ask before opening windows or playing audio.

## Graph mode contracts and limits

- `Project.graphNodes`/`graphLinks` are the editable topology; `Project.graph`
  stays the **negotiated** FFmpeg graph the Console shows. The recipe's `graph`
  field holds `active`, node descriptors (step/input/output uids, a source's
  `audioIndex`), port links and `positions`; parameters live in the step rows.
- Mutate the topology only through Project (`addGraphSource`,
  `addGraphOperation`, `connectGraph`, `disconnectGraph`, `moveGraphNode`,
  `removeGraphNode`, `routeTrack`, `useRow`). `syncGraphState()` runs before every
  undo snapshot and in `refreshDerived()`: it drops nodes whose step, input or
  output is gone, links to ports a node no longer has, and duplicates; new steps
  go in front of the selected output; new outputs share the first output's feed;
  positions of departed nodes are pruned. The models reconcile by uid.
- `Project::duration()` is the cached plan's longest output; the transport uses
  the selected output's. `renderInput()` (the selected input if it feeds the
  graph, else the first source) names outputs and supplies tags, chapters and
  cover. A/B needs the selected input to feed an aligned graph; when A/B is
  unavailable the transport switches to B (what a render writes).
- Duck pads both inputs and cuts to the main input with `amix duration=first`:
  plain `sidechaincompress` ends when either queue reports EOF, which varied
  between runs (1.877 s instead of 2 s).
- Seekable graphs (aligned, no convolution) seek every input with `-copyts`;
  crossfades, convolutions, trims and speed changes decode from the start to
  reach a seek point, so seeking in long files is slow in those graphs.
  Stateful filters (compressors, amix's normalization ramp) start fresh at a
  seek point, as in Row mode.
- Port-shaping options are listed under `fixed` in `steps.json`: hidden from the
  Inspector's Advanced section and refused by the compiler. Other options pass
  through quoted. Crossover frequencies must rise (1–7 of them).
- Preview converts a graph output to the selected input's channel count (a
  4-channel merge previews in stereo); renders keep the graph's channels.
- Batch folders can't be graph sources; queueing a graph renders that one
  graph. Chain presets hold one path, so branched graphs can't be saved as
  presets (save the project). The Deliver bar's output chips aren't drop targets
  (graph output cards are). FFmpeg 9's `acrossfade` takes N inputs; ffgui uses two.

## Code map

| Location | Responsibility |
|---|---|
| `src/core/tools/` | `ProcessJob` for every tool invocation, progress/log parsing, paths, quoting, the Console tokenizer. |
| `src/core/project/` | Recipe JSON (version 1; optional `edits` and `graph`) and snapshot undo. |
| `src/core/command/` | StepBuilders, ChainCompiler, CopyAdvisor, SizeEstimator, CommandBuilder (`track["graphPlan"]` selects the graph path) and GraphProbe (`runGraph`). |
| `src/core/graph/` | GraphCompiler and GraphPlan. |
| `src/core/render/`, `metadata/`, `capture/`, `compare/`, `transcript/`, `dsp/` | Renders and queue, cover/FFmetadata, recording and streams (`StreamSpec::graph`), comparison (`CompareSpec::graphB`), transcripts, scopes. |
| `src/core/media/`, `playback/` | `PcmReader` (graph decode: `graphInputs/Filter/Output/Seekable`), `PlaybackEngine::setProcessedGraph`. |
| `src/backend/Project.*` | Inputs/probes, rows, outputs, metadata edits, the graph topology, recipe/undo mutations. `ProjectEdit` coalesces nested mutations. |
| `src/backend/Engine.*` | Current-input analysis/decoding/playback, `updateChain`/`updateGraph`, render queue wiring, tap sharing, stopped scope reads. |
| `src/backend/Tools.*`, `ToolPlot.*` | Phase 3 tool facades and their textures. |
| `src/backend/Models.*`, `RowModel.*` | Fixed role lists are the QML contract; GraphModel adds `stepIndex,inputUid,outputUid,audioIndex,type,inputNames,outputNames,error`. |
| `qml/` | Feature modules import only Backend/Theme/Controls; Shell owns cross-panel dialogs, actions, drop targets and docking. |

## Next

The approved plan ends here; there is no Phase 5. After the user reviews
Phase 4, prioritize their reports, then the outstanding hand checks above
(listening, a pointer drag from Sources, real capture/loopback, remote
RTMP/Icecast/SRT, a Whisper-enabled FFmpeg). Stacked multi-input Media View
lanes stay outside the plan. Don't invent phases or widen scope without the
user's direction.

## Gotchas to preserve

- QML Repeaters must use a bounded list/count derived in one binding. A transient
  mismatch once created millions of delegates.
- Invokable calls in bindings need an explicit NOTIFY dependency (for example
  `model.revision` or `App.caps.state`). Use fixed models for live data rather
  than rebuilding delegates every meter tick.
- Custom scene items must draw texture images for the software renderer.
- `App` stays non-default-constructible so QML uses `App::create()` and installs
  the cover image provider.
- Avoid backend/QML type-name collisions, Item property/role collisions, and ids
  such as `palette`, `menu`, `scale`, `dock` that shadow their own type properties.
- Parse captured FFmpeg output, not imagined formats. GraphProbe's fixture has
  a duplicate `sample_rate` key; its tolerant parser handles this deliberately.
- New per-sample DSP needs `-O2` even in Debug (`src/core/CMakeLists.txt`).
- Cancel/disconnect owned jobs before dependent App services are destroyed.
- Do not let tests use real user settings, generated recordings, streams or
  audible playback. Tests/screenshots run with temporary configuration.
- A drag (DragHandler) replaces a card's position bindings; restore them after
  the move so undo moves the card back. Graph mutations that run inside another
  edit call `syncGraphState()` first, so nodes added earlier in that edit exist.
- QML test functions run in name order; `tst_workspaces.qml`'s
  `test_wired_graph` relies on running after `test_screenshot`.

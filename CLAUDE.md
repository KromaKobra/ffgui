# ffgui: notes for working in this repo

## Commands

- Build: `cmake --preset dev && cmake --build --preset dev`
- Everything (build, import check, qmllint, ctest): `scripts/run-tests.sh`. Must stay green; qmllint warnings fail it.
- Screenshots of every workspace (and the Quilt mock-up): `scripts/screenshots.sh`
- Run: `build/dev/src/app/ffgui`. Offscreen smoke run: `QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 timeout 5 build/dev/src/app/ffgui`
- Ask before opening windows on the user's desktop or playing audio aloud. Automated playback tests run at zero volume.

## Layering (enforced by scripts/check-imports.sh)

`Theme ← Controls ← feature modules (Sources, Media, Meters, Chain, Inspector, Output, Tools/*) ← Shell`

- `FFgui.Theme`: tokens only. Every colour comes from `Theme`, sizes from `Metrics`, icons from `Glyphs`. No hex literals anywhere else.
- `FFgui.Controls` imports only Theme, never Backend, so it stays reusable.
- Feature modules import Backend, Theme and Controls. Never Quilt, never each other. To open another panel: `App.requestPanel(id, popOut)`. For dialogs: `App.requestDialog(id, options)`; the Shell owns both.
- Feature modules and the Shell use the `Ff*` controls (`FfButton`, `FfToolButton`, `FfCheckBox`, `FfSwitch`, `FfSlider`, `FfComboBox`, `FfTextField`, `FfTextArea`, `FfSpinBox`, `FfRadioButton`, `FfMenu`, `FfMenuItem`, `FfMenuSeparator`, `FfItemDelegate`), never QtQuick.Controls inputs. When Quilt ships its own inputs, only those wrapper files change. Containers (ScrollView, Popup, ToolTip, TabBar, ProgressBar) are fine.
- Only `qml/Shell/DockShell.qml` imports Quilt (the check also allows WorkspaceController and PanelsMenu). Public API only: DockArea, DockPanel, DockStyle, DockWindow, Dock; openPanel, closePanel, popOut, toggleMaximized, saveLayout, restoreLayout, resetLayout, panel. Never `manager.*`. Panel order comes from `PanelRegistry.js`; pop-out windows register themselves in `DockShell.popouts`.

## Quilt

`third_party/Quilt` is a pristine copy made by `scripts/sync-quilt.sh` (records the commit in `third_party/QUILT_VERSION`). Never edit it. A Quilt change can only break DockShell's use of the public API and the Theme→DockStyle mapping there.

## Backend conventions

- `App` (QML singleton) owns everything as CONSTANT sub-objects: project, selection, transport, meters, analysis, render, queue, caps, log, undo, view, command, catalog, transcript, devices, destinations.
- List models are `RowModel` subclasses (`Models.h`). The role list in `Models.cpp` is the contract with QML delegates (`required property` per role). Update rows with `RowModel::update()` so only changed roles signal. To show one row in a sheet, use `ModelRow { model: ...; row: ... }`.
- Names must not clash between Backend types and QML files (Backend's `Transport` hid `Media/Transport.qml`; it is `TransportBar.qml` now). Model roles must not reuse Item property names (`state`, `index` on a StripedRow, `text` on a Text).
- Pure logic goes in `src/core` with QtTest coverage in `tests/core`; the Backend is a thin QML face over it.
- Things not built yet are API stubs that toast "isn't available yet". Grep for that text to find them.

## QML style

- `pragma ComponentBehavior: Bound`, ids qualified everywhere, `required property` for delegate roles.
- Ids must not shadow properties of the type they sit in (`palette`, `menu`, `scale`, `dock` all bit us).
- Plain names first, ffmpeg names second (tooltips, footers, subtitles).
- Every panel has an empty state: one sentence and the action that fills it.

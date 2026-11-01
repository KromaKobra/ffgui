pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// 2 · Media View: everything about the audio, drawn against one shared
// timeline. One lane per channel (waveform over spectrogram); analysis results
// are layers on the audio itself, toggled from the Layers menu. The Transport
// is part of this panel, so it follows the Media View into a pop-out window
// or a maximized state.
Item {
    id: view

    readonly property bool hasInput: App.project.hasInput
    readonly property real duration: Math.max(0.001, App.project.duration)
    readonly property var track: App.project.track
    readonly property int sampleRate: view.track.rate || 48000
    readonly property var layers: App.view.layers
    readonly property var results: App.analysis.results
    readonly property real labelWidth: Metrics.laneLabelWidth

    // The visible window, in seconds. Scroll zooms around the pointer.
    property real viewStart: 0
    property real viewEnd: view.duration
    readonly property real span: Math.max(1e-6, view.viewEnd - view.viewStart)
    property string rulerUnit: "timecode"
    property real bpm: 120
    property bool merged: false
    // Shift+scroll: waveform amplitude, or the spectrogram's top frequency.
    property real amplitude: 1
    property real maxFrequency: 0
    property bool logFrequency: true
    readonly property real target: isNaN(App.meters.target) ? -23 : App.meters.target

    readonly property var channelNames: {
        const n = view.track.channels || 0;
        if (view.merged || n === 1)
            return [n === 1 ? "M" : "Σ"];
        return n === 2 ? ["L", "R"] : (view.track.channelNames || []);
    }

    function setView(start, end) {
        const minSpan = Math.min(view.duration, 64 / view.sampleRate);
        let s = end - start < minSpan ? (start + end - minSpan) / 2 : start;
        let e = end - start < minSpan ? s + minSpan : end;
        if (e - s > view.duration) {
            s = 0;
            e = view.duration;
        }
        if (s < 0) {
            e -= s;
            s = 0;
        }
        if (e > view.duration) {
            s -= e - view.duration;
            e = view.duration;
        }
        view.viewStart = Math.max(0, s);
        view.viewEnd = e;
    }
    function zoom(factor, around) {
        view.setView(around - (around - view.viewStart) * factor, around + (view.viewEnd - around) * factor);
    }
    function fit() { view.setView(0, view.duration); }

    // The intervals ({ start, end }) that overlap the visible window, at most
    // 400: overlays never create an item per region of a long file.
    function visible_(intervals) {
        const out = [];
        for (const r of intervals) {
            if (r.end >= view.viewStart && r.start <= view.viewEnd)
                out.push(r);
            if (out.length >= 400)
                break;
        }
        return out;
    }

    Connections {
        target: App.project
        function onCurrentInputChanged() {
            view.fit();
            view.amplitude = 1;
            view.maxFrequency = 0;
        }
    }
    onDurationChanged: view.fit()

    // Zoomed past the peak pyramid's resolution: decode the visible samples.
    Timer {
        id: windowRequest
        interval: 120
        onTriggered: {
            const spp = view.span * view.sampleRate / Math.max(1, lanesArea.tracksWidth);
            if (spp < 256)
                App.media.requestWindow(Math.max(0, view.viewStart - view.span * 0.5), Math.min(view.duration, view.viewEnd + view.span * 0.5));
        }
    }
    onViewStartChanged: windowRequest.restart()
    onViewEndChanged: windowRequest.restart()

    Rectangle { anchors.fill: parent; color: Theme.canvas }

    QuickConvert {
        anchors.fill: parent
        visible: !view.hasInput
    }

    EmptyState {
        anchors.fill: parent
        visible: view.hasInput && (App.media.live || (!App.media.ready && !(App.project.input.probing ?? false)))
        glyph: App.media.live ? Glyphs.url : Glyphs.warning
        title: App.media.live ? qsTr("A live stream has no waveform") : qsTr("Nothing to show")
        text: App.media.live ? qsTr("Its length isn't known, so it can't be drawn or analysed. Open it in ffplay to listen.")
                             : App.project.input.healthText || qsTr("This input couldn't be read.")
        actionText: App.media.live ? qsTr("Open in ffplay") : ""
        onAction: App.openInFfplay()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: view.hasInput && App.media.ready

        // Title row: what is shown, ruler units, zoom, layers.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            Layout.leftMargin: Metrics.margin
            Layout.rightMargin: Metrics.spacingSmall
            spacing: Metrics.margin

            Label {
                text: App.project.input.name || ""
                font.bold: true
                elide: Text.ElideRight
                Layout.maximumWidth: 260
            }
            Caption {
                text: Format.signal({ rate: view.track.rate, fmt: view.track.sampleFmt, layout: view.track.layout })
                      + "   ·   " + Format.timecode(view.duration, true)
                Layout.fillWidth: true
            }
            FfComboBox {
                Layout.preferredWidth: 110
                implicitHeight: 24
                font.pixelSize: Metrics.fontCaption
                model: [qsTr("Timecode"), qsTr("Seconds"), qsTr("Samples"), qsTr("Bars")]
                currentIndex: ["timecode", "seconds", "samples", "bars"].indexOf(view.rulerUnit)
                tip: qsTr("Ruler units")
                onActivated: index => view.rulerUnit = ["timecode", "seconds", "samples", "bars"][index]
            }
            FfToolButton {
                text: view.span >= view.duration - 1e-6 ? qsTr("Zoom: fit")
                      : qsTr("Zoom: %1×").arg(Math.round(view.duration / view.span))
                font.pixelSize: Metrics.fontCaption
                tip: qsTr("Scroll to zoom time, Shift+scroll to zoom amplitude. Click to fit.")
                onClicked: view.fit()
            }
            FfToolButton {
                visible: view.layers.spectrogram === true
                text: view.logFrequency ? qsTr("Log Hz") : qsTr("Linear Hz")
                font.pixelSize: Metrics.fontCaption
                tip: qsTr("Spectrogram frequency axis. Shift+scroll over it to zoom frequencies.")
                onClicked: view.logFrequency = !view.logFrequency
            }
            FfToolButton {
                text: view.merged ? qsTr("Summed") : qsTr("Per channel")
                font.pixelSize: Metrics.fontCaption
                visible: (view.track.channels || 0) > 1
                checked: view.merged
                tip: qsTr("Show one lane per channel, or one summed lane")
                onClicked: view.merged = !view.merged
            }
            MenuButton {
                text: qsTr("Layers") + " " + Glyphs.open
                alignRight: true
                menu: LayersMenu { }
            }
        }

        OverviewStrip {
            Layout.fillWidth: true
            Layout.preferredHeight: Metrics.overviewHeight
            Layout.leftMargin: view.labelWidth
            Layout.rightMargin: Metrics.spacingSmall
            duration: view.duration
            viewStart: view.viewStart
            viewEnd: view.viewEnd
            position: App.transport.position
            onScrollTo: start => view.setView(start, start + view.span)
            content: WaveformItem {
                anchors.fill: parent
                media: App.media
                channel: -1
                viewStart: 0
                viewEnd: view.duration
                showRms: false
                peakColor: Theme.overview
            }
        }

        TimeRuler {
            id: ruler
            Layout.fillWidth: true
            Layout.preferredHeight: Metrics.rulerHeight
            Layout.leftMargin: view.labelWidth
            Layout.rightMargin: Metrics.spacingSmall
            viewStart: view.viewStart
            viewEnd: view.viewEnd
            unit: view.rulerUnit
            sampleRate: view.sampleRate
            bpm: view.bpm
            chapters: App.project.chapters
            showChapters: view.layers.chapters === true
        }

        Item {
            id: lanesArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.rightMargin: Metrics.spacingSmall
            clip: true

            readonly property bool showTranscript: view.layers.transcript === true && App.transcript.count > 0
            readonly property real extras: (view.layers.phase ? 16 : 0) + (showTranscript ? 24 : 0)
            readonly property real lanesHeight: height - extras
            readonly property real laneHeight: lanesHeight / Math.max(1, view.channelNames.length)
            readonly property real tracksWidth: width - view.labelWidth
            function xOf(t) { return view.labelWidth + (t - view.viewStart) / view.span * tracksWidth; }
            function tOf(x) {
                return Math.max(0, Math.min(view.duration, view.viewStart + (x - view.labelWidth) / tracksWidth * view.span));
            }

            Repeater {
                model: view.channelNames
                delegate: Lane {
                    required property int index
                    required property string modelData
                    channel: index
                    dataChannel: view.merged ? -1 : index
                    name: modelData
                    y: index * lanesArea.laneHeight
                    width: lanesArea.width
                    height: lanesArea.laneHeight
                    viewStart: view.viewStart
                    viewEnd: view.viewEnd
                    nyquist: view.sampleRate / 2
                    amplitude: view.amplitude
                    maxFrequency: view.maxFrequency
                    logFrequency: view.logFrequency
                    clips: view.results.clipping ? (view.results.clipping.times || []) : []
                    dc: view.results.levels && view.results.levels.dc && !view.merged ? (view.results.levels.dc[index] ?? NaN) : NaN
                }
            }

            // Phase / correlation lane, under the channel lanes.
            Item {
                id: phaseLane
                visible: view.layers.phase === true
                x: view.labelWidth
                y: lanesArea.lanesHeight + 2
                width: lanesArea.tracksWidth
                height: 12
                Rectangle { anchors.fill: parent; color: Theme.well }
                CurveItem {
                    anchors.fill: parent
                    analysis: App.analysis
                    series: "phase"
                    viewStart: view.viewStart
                    viewEnd: view.viewEnd
                    minValue: -1
                    maxValue: 1
                    bars: true
                    color: Theme.phaseGood
                    warnColor: Theme.phaseWarn
                    badColor: Theme.phaseBad
                    warnBelow: 0.5
                    badBelow: 0
                }
                Text {
                    x: -view.labelWidth + 8
                    y: -1
                    text: Glyphs.phase
                    color: Theme.textDim
                    font.pixelSize: Metrics.fontCaption
                }
            }

            // Transcript lane, aligned to speech.
            Item {
                id: transcriptLane
                visible: lanesArea.showTranscript
                x: view.labelWidth
                y: lanesArea.height - 22
                width: lanesArea.tracksWidth
                height: 20
                Repeater {
                    model: transcriptLane.visible ? App.transcript : null
                    delegate: Rectangle {
                        id: segment
                        required property real start
                        required property real end
                        required property string text
                        visible: end >= view.viewStart && start <= view.viewEnd
                        x: lanesArea.xOf(start) - view.labelWidth
                        width: Math.max(4, lanesArea.xOf(end) - lanesArea.xOf(start) - 2)
                        height: 18
                        radius: Metrics.radius
                        color: Theme.transcript
                        clip: true
                        Text {
                            anchors { fill: parent; leftMargin: 4 }
                            verticalAlignment: Text.AlignVCenter
                            text: segment.text
                            color: Theme.transcriptText
                            font.pixelSize: Metrics.fontSmall
                            elide: Text.ElideRight
                        }
                        TapHandler { onTapped: App.transport.seek(segment.start) }
                    }
                }
            }

            // Grid at the ruler's ticks (beats when the ruler shows bars).
            Repeater {
                model: view.layers.grid ? ruler.ticks : []
                delegate: Rectangle {
                    required property real modelData
                    x: lanesArea.xOf(modelData)
                    width: 1
                    height: lanesArea.lanesHeight
                    color: Theme.grid
                }
            }

            // Silence regions, with their length.
            Repeater {
                model: view.layers.silence && view.results.silence ? view.visible_(view.results.silence) : []
                delegate: Rectangle {
                    id: quiet
                    required property var modelData
                    visible: modelData.end >= view.viewStart && modelData.start <= view.viewEnd
                    x: lanesArea.xOf(modelData.start)
                    width: Math.max(1, lanesArea.xOf(modelData.end) - x)
                    height: lanesArea.lanesHeight
                    color: Theme.silence
                    Text {
                        visible: parent.width > 26
                        anchors { top: parent.top; topMargin: 3; horizontalCenter: parent.horizontalCenter }
                        text: Format.number(quiet.modelData.end - quiet.modelData.start) + " s"
                        color: Theme.textOnAccent
                        font.pixelSize: Metrics.fontTiny
                    }
                }
            }

            // Time ranges of chain steps; drag an edge to edit the range.
            Repeater {
                model: view.layers.ranges ? App.project.steps : null
                delegate: OverlayBand {
                    required property int index
                    required property bool hasRange
                    required property bool bypass
                    required property real rangeStart
                    required property real rangeEnd
                    required property string name
                    visible: hasRange && !bypass
                    height: lanesArea.lanesHeight
                    label: name
                    start: rangeStart
                    end: rangeEnd
                    xOf: t => lanesArea.xOf(t)
                    tOf: x => lanesArea.tOf(x)
                    onEdited: (s, e) => App.project.setRange(index, s, e)
                    onClicked: App.selection.select("step", { index: index })
                }
            }

            // Cut preview: what a Trim step removes.
            Repeater {
                model: view.layers.cut && (App.project.keptStart > 0 || (App.project.keptEnd > 0 && App.project.keptEnd < view.duration))
                       ? [[0, App.project.keptStart], [App.project.keptEnd, view.duration]] : []
                delegate: Rectangle {
                    required property var modelData
                    x: lanesArea.xOf(modelData[0])
                    width: Math.max(0, lanesArea.xOf(modelData[1]) - x)
                    height: lanesArea.lanesHeight
                    color: Theme.cut
                }
            }

            // Loudness: the short-term curve (-60 to 0 LUFS over the lanes),
            // the target line and the integrated value.
            Item {
                id: loudnessLayer
                visible: view.layers.loudness === true && view.results.loudness !== undefined
                x: view.labelWidth
                width: lanesArea.tracksWidth
                height: lanesArea.lanesHeight
                function yOf(lufs) { return (1 - (lufs + 60) / 60) * height; }
                CurveItem {
                    anchors.fill: parent
                    analysis: App.analysis
                    series: "shortTerm"
                    viewStart: view.viewStart
                    viewEnd: view.viewEnd
                    minValue: -60
                    maxValue: 0
                    color: Theme.loudness
                }
                Rectangle {
                    y: loudnessLayer.yOf(view.target)
                    width: parent.width
                    height: 1
                    color: Theme.target
                    opacity: 0.8
                }
                Text {
                    x: 6
                    y: loudnessLayer.yOf(view.target) - 14
                    text: qsTr("target %1 LUFS").arg(Format.number(view.target, 1))
                    color: Theme.target
                    font.pixelSize: Metrics.fontSmall
                }
            }
            Rectangle {
                visible: view.layers.loudness === true && view.results.loudness !== undefined
                anchors { right: parent.right; top: parent.top; margins: 6 }
                width: integrated.implicitWidth + 12
                height: 18
                radius: Metrics.radius
                color: Theme.loudnessBadge
                Text {
                    id: integrated
                    anchors.centerIn: parent
                    text: view.results.loudness
                          ? "I = " + Format.number(view.results.loudness.integrated) + " LUFS · LRA "
                            + Format.number(view.results.loudness.lra) + " LU"
                          : ""
                    color: Theme.loudness
                    font.pixelSize: Metrics.fontSmall
                    font.bold: true
                }
            }

            // Selection, with its start, end and length.
            Item {
                id: selectionBox
                visible: App.selection.isRange
                x: visible ? lanesArea.xOf(App.selection.start) : 0
                width: visible ? Math.max(1, lanesArea.xOf(App.selection.end) - x) : 0
                y: App.selection.channel >= 0 && !view.merged ? App.selection.channel * lanesArea.laneHeight : 0
                height: App.selection.channel >= 0 && !view.merged ? lanesArea.laneHeight : lanesArea.lanesHeight
                Rectangle { anchors.fill: parent; color: Theme.selectionFill }
                Rectangle { width: 1; height: parent.height; color: Theme.selectionEdge }
                Rectangle { x: parent.width - 1; width: 1; height: parent.height; color: Theme.selectionEdge }
                Rectangle {
                    y: parent.height - 20
                    x: Math.min(0, lanesArea.width - selectionBox.x - width)
                    width: selectionLabel.implicitWidth + 12
                    height: 18
                    radius: Metrics.radius
                    color: Theme.selectionLabel
                    Text {
                        id: selectionLabel
                        anchors.centerIn: parent
                        color: Theme.textOnAccent
                        font.pixelSize: Metrics.fontSmall
                        font.family: Metrics.monoFamily
                        text: selectionBox.visible
                              ? Format.timecode(App.selection.start, true) + " " + Glyphs.arrow + " "
                                + Format.timecode(App.selection.end, true) + "   ("
                                + Format.timecode(App.selection.end - App.selection.start, true) + ")"
                              : ""
                    }
                }
            }

            // Playhead
            Rectangle {
                visible: App.transport.position >= view.viewStart && App.transport.position <= view.viewEnd
                x: lanesArea.xOf(App.transport.position)
                width: 1.5
                height: lanesArea.height
                color: Theme.playhead
            }

            Crosshair {
                anchors.fill: parent
                visible: pointer.containsMouse && pointer.mouseX > view.labelWidth
                px: pointer.mouseX
                py: pointer.mouseY
                leftEdge: view.labelWidth
                text: visible ? pointer.readout() : ""
            }

            // Decoding progress, while the waveform fills in.
            Rectangle {
                visible: App.media.loading
                z: 3
                x: view.labelWidth + 6
                y: 6
                width: decodeLabel.implicitWidth + 16
                height: 20
                radius: Metrics.radius
                color: Theme.tooltip
                opacity: 0.8
                Text {
                    id: decodeLabel
                    anchors.centerIn: parent
                    text: qsTr("Decoding %1%").arg(Math.round(App.media.progress * 100))
                    color: Theme.textOnAccent
                    font.pixelSize: Metrics.fontSmall
                }
            }
            Text {
                visible: App.media.error !== "" && App.media.error !== "cancelled"
                z: 3
                x: view.labelWidth + 6
                y: 6
                text: Glyphs.warning + " " + qsTr("Decoding stopped: %1").arg(App.media.error)
                color: Theme.errorSoft
                font.pixelSize: Metrics.fontSmall
            }

            // Below the lanes (z: -1) so the spectrogram dividers stay draggable.
            MouseArea {
                id: pointer
                z: -1
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                property real pressT: 0
                property int pressChannel: -1
                property bool dragging: false

                function laneAt(y) { return Math.min(view.channelNames.length - 1, Math.floor(y / lanesArea.laneHeight)); }

                // Time (and level, once decoded) on the waveform; frequency, note
                // and time on the spectrogram.
                function readout() {
                    const t = lanesArea.tOf(mouseX);
                    if (mouseY > lanesArea.lanesHeight)
                        return Format.timecode(t, true);
                    const lane = laneAt(mouseY);
                    const fy = (mouseY - lane * lanesArea.laneHeight) / lanesArea.laneHeight;
                    const share = !view.layers.spectrogram ? 0 : !view.layers.waveform ? 1 : App.view.spectrogramShare;
                    const channel = view.merged ? -1 : lane;
                    const halfPixel = view.span / Math.max(1, lanesArea.tracksWidth) / 2;
                    if (fy < 1 - share) {
                        const level = App.media.levelAt(channel, t, halfPixel);
                        return Format.timecode(t, true) + "   " + (isNaN(level) ? "" : Format.dbValue(level) + " dBFS");
                    }
                    const top = view.maxFrequency > 0 ? Math.min(view.maxFrequency, view.sampleRate / 2) : view.sampleRate / 2;
                    const u = 1 - (fy - (1 - share)) / share;
                    const f = view.logFrequency ? 20 * Math.pow(top / 20, u) : 20 + (top - 20) * u;
                    const db = App.media.spectrumAt(channel, t, f);
                    return Format.timecode(t, true) + "   " + Format.frequency(f) + "   " + Format.note(f)
                           + (isNaN(db) ? "" : "   " + Format.dbValue(db, 0) + " dB");
                }

                onPressed: ev => {
                    pressT = lanesArea.tOf(ev.x);
                    pressChannel = (ev.modifiers & Qt.ShiftModifier) && ev.y < lanesArea.lanesHeight ? laneAt(ev.y) : -1;
                    dragging = false;
                }
                onPositionChanged: ev => {
                    if (!pressed)
                        return;
                    const t = lanesArea.tOf(ev.x);
                    if (!dragging && Math.abs(t - pressT) / view.span * lanesArea.tracksWidth > 4)
                        dragging = true;
                    if (dragging)
                        App.selection.select("range", { start: Math.min(pressT, t), end: Math.max(pressT, t), channel: pressChannel });
                }
                onReleased: {
                    if (dragging)
                        return;
                    App.transport.seek(pressT);
                    if (App.selection.isRange)
                        App.selection.clear();
                }
                onWheel: ev => {
                    const delta = ev.angleDelta.y !== 0 ? ev.angleDelta.y : ev.angleDelta.x;
                    if (ev.modifiers & Qt.ShiftModifier) {
                        // Over a waveform: amplitude. Over a spectrogram: its top frequency.
                        const lane = laneAt(ev.y);
                        const fy = (ev.y - lane * lanesArea.laneHeight) / lanesArea.laneHeight;
                        const share = !view.layers.spectrogram ? 0 : !view.layers.waveform ? 1 : App.view.spectrogramShare;
                        if (fy >= 1 - share) {
                            const nyquist = view.sampleRate / 2;
                            const top = view.maxFrequency > 0 ? view.maxFrequency : nyquist;
                            const next = top * (delta > 0 ? 0.8 : 1.25);
                            view.maxFrequency = next >= nyquist ? 0 : Math.max(200, next);
                        } else {
                            view.amplitude = Math.max(1, Math.min(64, view.amplitude * (delta > 0 ? 1.25 : 0.8)));
                        }
                        return;
                    }
                    view.zoom(delta > 0 ? 0.8 : 1.25, lanesArea.tOf(ev.x));
                }
            }

            SelectionToolbar {
                visible: selectionBox.visible && !pointer.pressed
                x: Math.max(view.labelWidth + 4, Math.min(selectionBox.x + selectionBox.width / 2 - width / 2, lanesArea.width - width - 4))
                y: 6
                onAction: id => {
                    if (id === "loop")
                        App.transport.loop = !App.transport.loop;
                    else
                        App.project.applySelectionAction(id, App.selection.start, App.selection.end);
                }
            }
        }

        TransportBar {
            Layout.fillWidth: true
        }
    }
}

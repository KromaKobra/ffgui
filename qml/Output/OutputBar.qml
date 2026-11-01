pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// 4 · Deliver: output targets, format controls, size estimate, copy and lossy
// badges, Render, and the exact ffmpeg command. It sits outside the dock
// (it can't close or pop out), which also puts the dock's bottom zone between
// the Chain and this bar.
Rectangle {
    id: bar

    property bool collapsed: false

    readonly property int index: App.project.currentOutput
    readonly property bool hasOutputs: App.project.outputs.count > 0
    readonly property var target: current.values

    ModelRow {
        id: current
        model: App.project.outputs
        row: bar.index
    }

    implicitHeight: content.implicitHeight + 14
    color: Theme.outputBar

    Rectangle { width: parent.width; height: 1; color: Theme.divider }

    ColumnLayout {
        id: content
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 7; leftMargin: 12; rightMargin: 12 }
        spacing: Metrics.spacing

        // Collapsed: the target summary and Render on one line.
        RowLayout {
            visible: bar.collapsed
            Layout.fillWidth: true
            spacing: Metrics.margin
            StageBadge { number: 4; title: qsTr("Deliver") }
            Label {
                text: !bar.hasOutputs ? qsTr("No output yet")
                      : (bar.target.fileName || "") + " · " + (bar.target.codecLabel || "") + " · ≈ " + (bar.target.sizeText || "—")
                        + (App.project.outputs.count > 1 ? "   " + qsTr("(+%1 more)").arg(App.project.outputs.count - 1) : "")
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            RenderButton { }
            FfToolButton { text: Glyphs.up; tip: qsTr("Expand"); onClicked: bar.collapsed = false }
        }

        // Rendering: progress replaces the controls.
        RowLayout {
            visible: !bar.collapsed && App.render.active
            Layout.fillWidth: true
            spacing: Metrics.spacingWide
            StageBadge { number: 4; title: qsTr("Deliver") }
            Label { text: App.render.label || (App.render.fileCount === 1 ? qsTr("Rendering 1 file") : qsTr("Rendering %1 files").arg(App.render.fileCount)) }
            ProgressBar { value: App.render.progress; Layout.fillWidth: true }
            Label {
                font.family: Metrics.monoFamily
                text: Math.round(App.render.progress * 100) + "%   " + Format.number(App.render.speed, 0) + "×   "
                      + qsTr("%1 left").arg(Format.timecode(App.render.eta))
            }
            FfButton {
                text: qsTr("Stop")
                tip: qsTr("Stops cleanly: the files stay valid")
                onClicked: App.render.stop()
            }
        }

        // No target yet: pick a format (the quick-convert path).
        RowLayout {
            visible: !bar.collapsed && !App.render.active && !bar.hasOutputs
            Layout.fillWidth: true
            spacing: Metrics.spacingLarge
            StageBadge { number: 4; title: qsTr("Deliver") }
            Caption { text: qsTr("Choose a format:"); font.pixelSize: Metrics.fontBody }
            Repeater {
                model: App.catalog.presets
                delegate: FfButton {
                    required property string name
                    required property string description
                    text: name
                    flat: true
                    tip: description
                    onClicked: App.project.addOutput(name)
                }
            }
            Item { Layout.fillWidth: true }
        }

        // Targets and format controls. What doesn't fit is clipped, and the
        // signal format moves to the Inspector when space is short.
        RowLayout {
            visible: !bar.collapsed && !App.render.active && bar.hasOutputs
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            clip: true
            spacing: Metrics.spacingLarge

            StageBadge { number: 4; title: qsTr("Deliver") }

            Repeater {
                model: App.project.outputs
                delegate: TargetChip { }
            }
            FfToolButton {
                text: Glyphs.add
                tip: qsTr("Another output from the same project")
                onClicked: App.project.addOutput()
            }

            ToolSeparator { }

            FormatControls { index: bar.index; target: bar.target }

            RowLayout {
                id: signalGroup
                visible: bar.width >= 1480
                Layout.fillWidth: false
                spacing: Metrics.spacingLarge
                ToolSeparator { }
                SignalFormat { index: bar.index; target: bar.target }
                ToolSeparator { }
                MetadataPolicySelector { index: bar.index; current: bar.target.metadata || "copy" }
            }
            FfToolButton {
                visible: !signalGroup.visible
                text: qsTr("Signal && tags…")
                tip: qsTr("Sample rate, bit depth, channels and metadata, in the Inspector")
                onClicked: App.selection.select("output", { index: bar.index })
            }

            Item { Layout.fillWidth: true }
        }

        // Command preview, readouts, Render.
        RowLayout {
            visible: !bar.collapsed
            Layout.fillWidth: true
            spacing: Metrics.spacingLarge

            CommandPreview { Layout.fillWidth: true }

            ToolSeparator { }

            Label {
                visible: bar.hasOutputs
                text: "≈ " + (bar.target.sizeText || "—")
                font.family: Metrics.monoFamily
                HoverHandler { id: sizeHover }
                TipLabel {
                    visible: sizeHover.hovered
                    text: qsTr("Bitrate × duration. All outputs: %1").arg(App.project.totalSizeText)
                }
            }
            Badge {
                visible: bar.hasOutputs && App.project.hasInput
                text: bar.target.reencodeReason ? qsTr("RE-ENCODE") : qsTr("COPY")
                tone: bar.target.reencodeReason ? Theme.neutral : Theme.okDeep
                tip: bar.target.reencodeReason ? "" : qsTr("No re-encode: the audio is copied as it is")
            }
            Caption {
                visible: bar.hasOutputs && !!bar.target.reencodeReason
                text: bar.target.reencodeReason || ""
                Layout.maximumWidth: 190
            }
            Badge {
                visible: bar.target.lossyToLossy === true
                text: qsTr("LOSSY → LOSSY")
                tone: Theme.warn
                textColor: Theme.textOnWarn
                tip: qsTr("The source is already lossy; encoding it again loses more quality")
            }
            Badge {
                visible: !!bar.target.problem
                text: bar.target.problem || ""
                tone: Theme.errorDeep
                textColor: Theme.textOnAccent
            }

            RenderButton { }

            FfToolButton {
                text: Glyphs.open
                tip: qsTr("Collapse to one line")
                onClicked: bar.collapsed = true
            }
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Queue: batch jobs, one row per file × output, with status, progress, speed
// and errors. Reorder, pause, retry.
Item {
    id: panel

    function tone(status) {
        return status === "done" ? Theme.okDeep : status === "running" ? Theme.accent
             : status === "error" ? Theme.errorDeep : Theme.neutral;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PanelToolbar {
            Caption {
                text: qsTr("%1 jobs · %2 pending").arg(App.queue.count).arg(App.queuePending)
                Layout.fillWidth: true
            }
            FfButton {
                text: App.queue.paused ? qsTr("Resume") : qsTr("Pause all")
                font.pixelSize: Metrics.fontCaption
                enabled: App.queuePending > 0
                onClicked: App.queue.paused ? App.queue.resumeAll() : App.queue.pauseAll()
            }
            FfButton {
                text: qsTr("Clear finished")
                font.pixelSize: Metrics.fontCaption
                enabled: App.queue.count > App.queuePending
                onClicked: App.queue.clearFinished()
            }
        }

        EmptyState {
            visible: App.queue.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("No jobs")
            text: qsTr("Use Render ▾ › Add to queue, or add a folder as an input to convert many files with one recipe.")
        }

        ListView {
            visible: App.queue.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.queue
            ScrollBar.vertical: ScrollBar { }
            delegate: StripedRow {
                id: job
                required property int index
                row: job.index
                required property string file
                required property string output
                required property string status
                required property real progress
                required property string speed
                required property string error
                width: ListView.view.width
                height: 30
                RowLayout {
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    spacing: Metrics.margin
                    Label {
                        text: job.file + "  " + Glyphs.arrow + "  " + job.output
                        font.pixelSize: Metrics.fontCaption
                        elide: Text.ElideMiddle
                        Layout.preferredWidth: 280
                    }
                    Badge { text: job.status.toUpperCase(); tone: panel.tone(job.status) }
                    ProgressStrip {
                        visible: job.status === "running" || job.status === "done"
                        value: job.progress
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: job.error !== ""
                        text: job.error
                        color: Theme.errorSoft
                        font.pixelSize: Metrics.fontCaption
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Item { visible: job.status !== "running" && job.status !== "done" && job.error === ""; Layout.fillWidth: true }
                    Label { text: job.speed; font.pixelSize: Metrics.fontCaption; font.family: Metrics.monoFamily; opacity: 0.7 }
                    MenuButton {
                        text: job.status === "error" ? qsTr("Retry") : Glyphs.more
                        font.pixelSize: Metrics.fontCaption
                        alignRight: true
                        menu: FfMenu {
                            FfMenuItem { text: qsTr("Retry"); enabled: job.status === "error" || job.status === "stopped"; onTriggered: App.queue.retry(job.index) }
                            FfMenuItem { text: qsTr("Move up"); enabled: job.index > 0; onTriggered: App.queue.moveUp(job.index) }
                            FfMenuItem { text: qsTr("Move down"); enabled: job.index < App.queue.count - 1; onTriggered: App.queue.moveDown(job.index) }
                            FfMenuSeparator { }
                            FfMenuItem { text: qsTr("Cancel"); enabled: job.status === "queued" || job.status === "running"; onTriggered: App.queue.cancel(job.index) }
                        }
                    }
                }
            }
        }
    }
}

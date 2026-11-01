import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// One line at the very bottom: background analysis, the queue, warning and
// error counts (click for the Console) and the build badge (click for
// Capabilities, in its own window).
Rectangle {
    id: bar

    implicitHeight: Metrics.statusHeight
    color: Theme.statusBar

    component Link: Label {
        id: link
        signal activated()
        font.pixelSize: Metrics.fontCaption
        opacity: linkHover.hovered ? 1 : 0.8
        HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: link.activated() }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Metrics.margin
        anchors.rightMargin: Metrics.margin
        spacing: 14

        RowLayout {
            spacing: Metrics.spacing
            ProgressStrip {
                visible: App.analysis.running
                value: App.analysis.progress
                Layout.preferredWidth: 70
            }
            Link {
                text: App.analysis.running
                      ? qsTr("Analyzing %1 — %2 %3%").arg(App.analysis.subject).arg(App.analysis.stage).arg(Math.round(App.analysis.progress * 100))
                      : App.analysis.state === "done" ? Glyphs.check + " " + qsTr("Analysis done: %1").arg(App.analysis.subject)
                      : App.analysis.state === "failed" ? Glyphs.warning + " " + qsTr("Analysis failed: %1").arg(App.analysis.subject)
                      : App.project.hasInput ? qsTr("Analysis waiting") : qsTr("No input")
                onActivated: App.requestPanel("analysis")
            }
        }

        Link {
            text: qsTr("Queue %1").arg(App.queuePending)
            onActivated: App.requestPanel("queue")
        }

        Link {
            text: Glyphs.warning + " " + App.warningCount + "    " + Glyphs.cross + " " + App.errorCount
            color: App.errorCount ? Theme.errorSoft : App.warningCount ? Theme.warn : Theme.textDim
            onActivated: App.requestPanel("console")
        }

        Item { Layout.fillWidth: true }

        Link {
            text: App.caps.summary
            color: App.caps.state === "missing" ? Theme.errorSoft : Theme.text
            onActivated: App.requestPanel("capabilities", true)
        }
    }
}

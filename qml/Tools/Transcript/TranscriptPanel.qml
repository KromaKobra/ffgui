pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Transcript: text segments synced to the playhead. Clicking a line seeks
// there. Export as SRT, TXT or JSON; import a transcript made elsewhere.
Item {
    id: panel

    readonly property bool whisper: App.caps.hasFilter("whisper")
    readonly property int current: {
        let k = -1;
        for (let i = 0; i < App.transcript.count; ++i)
            if (App.transcript.value(i, "start") <= App.transport.position)
                k = i;
        return App.transcript.revision >= 0 ? k : -1;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        EmptyState {
            visible: App.transcript.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            glyph: "✎"
            title: qsTr("No transcript for this input yet")
            text: panel.whisper ? qsTr("Add Speech › Transcribe to the Chain, or import an SRT, TXT or JSON transcript.")
                                : qsTr("This FFmpeg build has no whisper filter, so it can't transcribe. You can still import an SRT, TXT or JSON transcript and edit it here.")
            actionText: qsTr("Import…")
            onAction: App.requestDialog("importTranscript")
        }

        ListView {
            id: list
            visible: App.transcript.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            currentIndex: panel.current
            highlightFollowsCurrentItem: true
            highlightMoveDuration: 200
            highlight: Rectangle { color: Theme.selected; radius: Metrics.radius }
            model: App.transcript
            ScrollBar.vertical: ScrollBar { }
            delegate: Item {
                id: line
                required property int index
                required property real start
                required property string text
                width: ListView.view.width
                height: body.implicitHeight + 10
                RowLayout {
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    spacing: Metrics.margin
                    Text {
                        text: Format.timecode(line.start)
                        color: Theme.textDim
                        font.family: Metrics.monoFamily
                        font.pixelSize: Metrics.fontSmall
                        Layout.alignment: Qt.AlignTop
                    }
                    Text {
                        id: body
                        text: line.text
                        color: line.index === panel.current ? Theme.textBright : Theme.textSoft
                        font.pixelSize: Metrics.fontBody
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
                TapHandler { onTapped: App.transport.seek(line.start) }
            }
        }

        ButtonRow {
            visible: App.transcript.count > 0
            FfButton { text: qsTr("Export SRT…"); font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("exportTranscript", { format: "srt" }) }
            FfButton { text: "TXT…"; font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("exportTranscript", { format: "txt" }) }
            FfButton { text: "JSON…"; font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("exportTranscript", { format: "json" }) }
        }
    }
}

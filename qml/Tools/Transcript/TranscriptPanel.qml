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

    // Reading state makes this re-check once the capabilities arrive.
    readonly property bool whisper: App.caps.state === "ready" && App.caps.hasFilter("whisper")
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

        RowLayout {
            visible: panel.whisper
            Layout.fillWidth: true
            FfTextField { id: whisperModel; Layout.fillWidth: true; placeholderText: qsTr("whisper.cpp model path"); enabled: !App.transcript.active }
            FfTextField { id: whisperLanguage; Layout.preferredWidth: 65; text: "auto"; enabled: !App.transcript.active }
            FfButton { text: App.transcript.active ? qsTr("Cancel") : qsTr("Transcribe"); enabled: App.transcript.active || App.project.hasInput; onClicked: App.transcript.active ? App.transcript.cancel() : App.transcript.transcribe(whisperModel.text, whisperLanguage.text) }
        }
        Hint { visible: App.transcript.error !== ""; text: App.transcript.error; Layout.fillWidth: true }
        Caption { text: qsTr("Untimed TXT lines get five seconds each. Edit the times after import."); Layout.fillWidth: true }
        EmptyState {
            visible: App.transcript.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            glyph: "✎"
            title: qsTr("No transcript for this input yet")
            text: panel.whisper ? qsTr("Choose a whisper.cpp model above, or import an SRT, TXT or JSON transcript.")
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
                required property real end
                required property string text
                width: ListView.view.width
                height: body.implicitHeight + 44
                RowLayout {
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    spacing: Metrics.margin
                    Text {
                        text: Format.timecode(line.start)
                        color: Theme.textDim
                        font.family: Metrics.monoFamily
                        font.pixelSize: Metrics.fontSmall
                        Layout.alignment: Qt.AlignTop
                        TapHandler { onTapped: App.transport.seek(line.start) }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        RowLayout {
                            TimeField { Layout.preferredWidth: 95; seconds: line.start; display: Format.timecode(seconds, true); onEdited: v => App.transcript.setSegment(line.index, v, line.end, line.text) }
                            TimeField { Layout.preferredWidth: 95; seconds: line.end; display: Format.timecode(seconds, true); onEdited: v => App.transcript.setSegment(line.index, line.start, v, line.text) }
                            FfToolButton { text: Glyphs.remove; onClicked: App.transcript.removeSegment(line.index) }
                        }
                        FfTextArea {
                            id: body
                            Layout.fillWidth: true
                            text: line.text
                            color: line.index === panel.current ? Theme.textBright : Theme.textSoft
                            font.pixelSize: Metrics.fontBody
                            wrapMode: TextEdit.Wrap
                            onActiveFocusChanged: if (!activeFocus && text !== line.text) App.transcript.setSegment(line.index, line.start, line.end, text)
                        }
                    }
                }

            }
        }

        ButtonRow {
            FfButton { text: qsTr("Add line"); font.pixelSize: Metrics.fontCaption; onClicked: App.transcript.addSegment(App.transport.position, App.transport.position + 5, "") }
            FfButton { text: qsTr("Import…"); font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("importTranscript") }
            FfButton { text: qsTr("Export SRT…"); font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("exportTranscript", { format: "srt" }) }
            FfButton { text: "TXT…"; font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("exportTranscript", { format: "txt" }) }
            FfButton { text: "JSON…"; font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("exportTranscript", { format: "json" }) }
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The live log of every ffmpeg and ffprobe run, with a level filter.
ColumnLayout {
    id: tab

    property string level: "all"

    spacing: Metrics.spacingSmall

    RowLayout {
        Layout.margins: Metrics.spacing
        Caption { text: qsTr("Level") }
        SegmentedControl {
            options: [{ value: "all", label: qsTr("All") }, { value: "info", label: qsTr("Info") },
                      { value: "warning", label: qsTr("Warnings") }, { value: "error", label: qsTr("Errors") }]
            current: tab.level
            fontSize: Metrics.fontCaption
            onActivated: value => tab.level = value
        }
        Item { Layout.fillWidth: true }
        FfToolButton {
            text: qsTr("Copy all")
            font.pixelSize: Metrics.fontCaption
            enabled: App.log.count > 0
            onClicked: {
                const lines = [];
                for (let i = 0; i < App.log.count; ++i) {
                    const e = App.log.get(i);
                    lines.push(e.time + " [" + e.level + "] " + e.text);
                }
                App.copyText(lines.join("\n"));
            }
        }
    }

    Hint {
        visible: App.log.count === 0
        Layout.margins: Metrics.margin
        text: qsTr("Nothing logged yet. Every ffmpeg and ffprobe run writes its output here, with the exact command.")
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: Metrics.spacing
        clip: true
        model: App.log
        ScrollBar.vertical: ScrollBar { }
        onCountChanged: if (atYEnd || count < 3) Qt.callLater(list.positionViewAtEnd)
        delegate: Item {
            id: entry
            required property string time
            required property string level
            required property string text
            width: ListView.view.width
            visible: tab.level === "all" || entry.level === tab.level
            height: visible ? line.implicitHeight : 0
            Text {
                id: line
                width: parent.width
                text: entry.time + "  [" + entry.level + "] " + entry.text
                color: entry.level === "error" ? Theme.errorSoft : entry.level === "warning" ? Theme.warn : Theme.textSoft
                font.family: Metrics.monoFamily
                font.pixelSize: Metrics.fontCaption
                wrapMode: Text.WrapAnywhere
            }
        }
    }
}

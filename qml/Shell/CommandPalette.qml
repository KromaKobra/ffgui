pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Controls
import FFgui.Theme

// Ctrl+K: every feature, panel and setting, searchable by plain name or by
// ffmpeg name.
Popup {
    id: commandPalette

    required property ActionRegistry registry
    property var results: []

    function show(text) {
        query.text = text || "";
        commandPalette.open();
    }
    function run(index) {
        const a = commandPalette.results[index];
        if (!a)
            return;
        commandPalette.close();
        a.run();
    }

    width: Math.min(620, parent ? parent.width - 40 : 620)
    height: Math.min(440, parent ? parent.height - 80 : 440)
    x: parent ? (parent.width - width) / 2 : 0
    y: 60
    modal: true
    focus: true
    padding: 1
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: {
        commandPalette.results = commandPalette.registry.search(query.text);
        list.currentIndex = 0;
        query.forceActiveFocus();
        query.cursorPosition = query.text.length;
    }

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.borderStrong
        radius: Metrics.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 0

        FfTextField {
            id: query
            Layout.fillWidth: true
            Layout.margins: Metrics.spacingLarge
            placeholderText: qsTr("Type a feature, panel or filter: “noise”, “afftdn”, “loudness”…")
            font.pixelSize: Metrics.fontLarge
            onTextChanged: {
                commandPalette.results = commandPalette.registry.search(text);
                list.currentIndex = 0;
            }
            Keys.onDownPressed: list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1)
            Keys.onUpPressed: list.currentIndex = Math.max(0, list.currentIndex - 1)
            Keys.onReturnPressed: commandPalette.run(list.currentIndex)
            Keys.onEnterPressed: commandPalette.run(list.currentIndex)
        }

        Divider { }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: commandPalette.results
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar { }
            delegate: FfItemDelegate {
                id: row
                required property int index
                required property var modelData
                width: ListView.view.width
                highlighted: ListView.isCurrentItem
                onClicked: commandPalette.run(row.index)
                contentItem: RowLayout {
                    spacing: Metrics.spacingLarge
                    Label { text: row.modelData.title; font.pixelSize: Metrics.fontBody; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label {
                        text: row.modelData.subtitle
                        font.family: Metrics.monoFamily
                        font.pixelSize: Metrics.fontSmall
                        opacity: 0.5
                        elide: Text.ElideRight
                        Layout.maximumWidth: 220
                    }
                    Caption { text: row.modelData.category }
                }
            }
        }

        Hint {
            visible: list.count === 0
            Layout.margins: Metrics.margin
            text: qsTr("Nothing matches “%1”.").arg(query.text)
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Opened from a + in the Chain: categories on the left, steps on the right
// with one-line descriptions. Typing searches every category by plain or
// ffmpeg name. Steps this build can't run are greyed out, naming what's missing.
Popup {
    id: picker

    property int insertAt: 0
    property string category: App.catalog.categories.length ? App.catalog.categories[0] : ""

    width: 560
    height: 340
    margins: Metrics.spacingLarge
    padding: 1
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onClosed: App.project.cancelPendingRange()

    onOpened: {
        searchField.text = "";
        searchField.forceActiveFocus();
    }

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.borderStrong
        radius: Metrics.radiusLarge
    }

    StepFilter {
        id: filtered
        sourceModel: App.catalog.steps
        category: searchField.text.trim() === "" ? picker.category : ""
        search: searchField.text
    }

    contentItem: ColumnLayout {
        spacing: 0

        FfTextField {
            id: searchField
            objectName: "stepSearch"
            Layout.fillWidth: true
            Layout.margins: Metrics.spacing
            placeholderText: qsTr("Search steps: “noise”, “afftdn”, “louder”…")
            Keys.onReturnPressed: {
                if (filtered.count > 0 && filtered.get(0).available) {
                    App.project.insertStep(picker.insertAt, filtered.get(0).key);
                    picker.close();
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ListView {
                Layout.preferredWidth: 170
                Layout.fillHeight: true
                enabled: searchField.text.trim() === ""
                opacity: enabled ? 1 : 0.4
                model: App.catalog.categories
                delegate: FfItemDelegate {
                    required property string modelData
                    width: ListView.view.width
                    text: modelData
                    highlighted: picker.category === modelData
                    onClicked: picker.category = modelData
                }
            }

            Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Theme.border }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: filtered
                ScrollBar.vertical: ScrollBar { }
                delegate: FfItemDelegate {
                    id: entry
                    required property string key
                    required property string name
                    required property string glyph
                    required property string filter
                    required property string blurb
                    required property bool available
                    required property string reason
                    width: ListView.view.width
                    enabled: entry.available
                    onClicked: {
                        App.project.insertStep(picker.insertAt, entry.key);
                        picker.close();
                    }
                    contentItem: ColumnLayout {
                        spacing: 1
                        RowLayout {
                            spacing: Metrics.spacingLarge
                            Label { text: entry.glyph + "   " + entry.name; font.bold: true; font.pixelSize: Metrics.fontBody }
                            Label {
                                text: entry.filter
                                font.family: Metrics.monoFamily
                                font.pixelSize: Metrics.fontSmall
                                opacity: 0.5
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Label {
                            text: entry.available ? entry.blurb : entry.reason
                            color: entry.available ? Theme.text : Theme.warn
                            opacity: 0.7
                            font.pixelSize: Metrics.fontCaption
                        }
                    }
                }
            }
        }
    }
}

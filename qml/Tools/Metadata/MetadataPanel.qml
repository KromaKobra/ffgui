pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Metadata: the tag editor (common fields as a form, other keys as a list),
// cover art, and chapters. Used by outputs whose metadata policy is "Edit".
Item {
    id: panel

    readonly property var tags: App.project.tags
    readonly property var common: ["title", "artist", "album", "date", "genre", "comment"]
    readonly property var custom: Object.keys(panel.tags).filter(k => panel.common.indexOf(k) < 0)
    readonly property var chapters: App.project.chapters

    EmptyState {
        anchors.fill: parent
        visible: !App.project.hasInput
        title: qsTr("No metadata yet")
        text: qsTr("Add an input: its tags, cover art and chapters show here, ready to edit for the outputs.")
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        visible: App.project.hasInput
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Metrics.spacingLarge

            Section {
                Layout.topMargin: Metrics.spacing
                title: qsTr("Tags")
                Repeater {
                    model: panel.common.concat(panel.custom)
                    delegate: FormRow {
                        id: tagRow
                        required property string modelData
                        label: modelData
                        labelWidth: 60
                        FfTextField {
                            Layout.fillWidth: true
                            text: panel.tags[tagRow.modelData] || ""
                            placeholderText: "—"
                            onEditingFinished: if (text !== (panel.tags[tagRow.modelData] || "")) App.project.setTag(tagRow.modelData, text)
                        }
                        FfToolButton { text: Glyphs.remove; tip: qsTr("Remove tag"); onClicked: App.project.removeTag(tagRow.modelData) }
                    }
                }
                RowLayout {
                    FfTextField { id: newKey; placeholderText: qsTr("custom key"); Layout.preferredWidth: 110 }
                    FfButton {
                        text: qsTr("+ Add")
                        flat: true
                        enabled: newKey.text.trim() !== ""
                        onClicked: {
                            App.project.setTag(newKey.text.trim(), "");
                            newKey.text = "";
                        }
                    }
                }
            }

            Section {
                title: qsTr("Cover art")
                Item {
                    Layout.fillWidth: true
                    implicitHeight: 96
                    DashedRect { anchors.fill: parent }
                    Image {
                        id: cover
                        x: 10
                        y: 10
                        width: 76
                        height: 76
                        source: App.project.cover
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Label {
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                                  leftMargin: cover.status === Image.Ready ? 98 : 10; rightMargin: 10 }
                        wrapMode: Text.Wrap
                        font.pixelSize: Metrics.fontCaption
                        opacity: 0.6
                        text: cover.status === Image.Ready
                              ? qsTr("%1×%2 attached picture").arg(cover.sourceSize.width).arg(cover.sourceSize.height)
                              : qsTr("Drop an image here to use it as cover art")
                    }
                }
                RowLayout {
                    FfButton { text: qsTr("Extract…"); enabled: cover.status === Image.Ready; onClicked: App.requestDialog("extractCover") }
                    FfButton { text: qsTr("Replace…"); onClicked: App.requestDialog("replaceCover") }
                    FfButton { text: qsTr("Remove"); enabled: cover.status === Image.Ready; onClicked: App.project.removeCover() }
                }
            }

            Section {
                title: qsTr("Chapters (%1)").arg(panel.chapters.length)
                Repeater {
                    model: panel.chapters.slice(0, 200)
                    delegate: RowLayout {
                        id: chapterRow
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        TimeField {
                            seconds: chapterRow.modelData.start
                            display: Format.timecode(seconds, true)
                            Layout.preferredWidth: 100
                            onEdited: v => App.project.setChapter(chapterRow.index, v, chapterRow.modelData.end, chapterRow.modelData.title || "")
                        }
                        TimeField {
                            seconds: chapterRow.modelData.end
                            display: Format.timecode(seconds, true)
                            Layout.preferredWidth: 100
                            onEdited: v => App.project.setChapter(chapterRow.index, chapterRow.modelData.start, v, chapterRow.modelData.title || "")
                        }
                        FfTextField {
                            Layout.fillWidth: true
                            text: chapterRow.modelData.title || ""
                            onEditingFinished: App.project.setChapter(chapterRow.index, chapterRow.modelData.start, chapterRow.modelData.end, text)
                        }
                        FfToolButton { text: Glyphs.remove; onClicked: App.project.removeChapter(chapterRow.index) }
                    }
                }
                FfButton { text: qsTr("Add chapter at playhead"); onClicked: App.project.addChapter(App.transport.position, Math.max(App.transport.position + 0.1, App.project.duration), qsTr("Chapter")) }
                Hint { visible: panel.chapters.length === 0; text: qsTr("No chapters. Add markers in the Media View to create them.") }
            }
            Section {
                title: qsTr("Track metadata")
                Repeater {
                    model: App.project.editableTracks
                    delegate: ColumnLayout {
                        id: trackEntry
                        required property var modelData
                        Layout.fillWidth: true
                        Caption { text: qsTr("Audio track %1").arg(trackEntry.modelData.audioIndex + 1) }
                        FfTextField { Layout.fillWidth: true; placeholderText: qsTr("Title"); text: trackEntry.modelData.title || ""; onEditingFinished: App.project.setTrackMetadata(trackEntry.modelData.audioIndex, { title: text }) }
                        FfTextField { Layout.fillWidth: true; placeholderText: qsTr("Language, e.g. eng"); text: trackEntry.modelData.lang || ""; onEditingFinished: App.project.setTrackMetadata(trackEntry.modelData.audioIndex, { lang: text }) }
                        FfCheckBox { text: qsTr("Default track"); checked: !!trackEntry.modelData.isDefault; onToggled: App.project.setTrackMetadata(trackEntry.modelData.audioIndex, { isDefault: checked }) }
                    }
                }
                Hint { text: qsTr("Choose Edit in an output's metadata policy to write these changes.") }
            }
        }
    }
}

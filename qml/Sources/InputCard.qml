pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// One input: file name, container badge, duration, size, cover, health dot.
// Expanded, it lists the tracks and (collapsible) chapters. Clicking selects
// it, so the Inspector and the Media View follow.
Card {
    id: card

    required property int index
    required property string name
    required property string path
    required property string kind
    required property string container
    required property real duration
    required property real size
    required property string health
    required property string healthText
    required property var tracks
    required property var chapters
    required property int count
    required property string cover
    required property bool probing

    property bool expanded: card.index === App.project.currentInput
    property bool chaptersOpen: false

    current: App.project.currentInput === card.index
    selected: (App.selection.kind === "input" || App.selection.kind === "track") && App.selection.index === card.index
    implicitHeight: body.implicitHeight + 12

    function select() {
        App.project.currentInput = card.index;
        App.selection.select("input", { index: card.index });
    }

    TapHandler { onTapped: card.select() }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: point => cardMenu.popup(card, point.position.x, point.position.y)
    }
    FfMenu {
        id: cardMenu
        FfMenuItem {
            text: qsTr("Reveal in folder")
            enabled: card.path !== ""
            onTriggered: App.revealInFolder(card.path)
        }
        FfMenuItem { text: qsTr("Re-probe"); onTriggered: App.project.reprobe(card.index) }
        FfMenuSeparator { }
        FfMenuItem { text: qsTr("Remove"); onTriggered: App.project.removeInput(card.index) }
    }

    ColumnLayout {
        id: body
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 }
        spacing: Metrics.spacingSmall

        // Card face
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingLarge

            CoverTile { cover: card.cover; kind: card.kind }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingTight
                Label {
                    text: card.name
                    font.bold: true
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacing
                    Badge { visible: card.container !== ""; text: card.container }
                    Caption {
                        Layout.fillWidth: true
                        text: card.probing ? qsTr("Reading…")
                              : [Format.timecode(card.duration),
                                 card.size > 0 ? Format.bytes(card.size) : "",
                                 card.count > 0 ? qsTr("%n files", "", card.count) : ""].filter(s => s !== "").join("  ·  ")
                    }
                }
            }

            // Green decodes cleanly, amber warnings, red errors. Opens Analysis.
            StatusDot {
                status: card.health
                tip: card.healthText + "\n" + qsTr("Click for the full analysis")
                onClicked: {
                    card.select();
                    App.requestPanel("analysis");
                }
            }
            FfToolButton {
                text: card.expanded ? Glyphs.open : Glyphs.closed
                implicitWidth: 24
                onClicked: card.expanded = !card.expanded
            }
        }

        Repeater {
            model: card.expanded ? card.tracks : []
            delegate: TrackRow {
                required property int index
                required property var modelData
                inputIndex: card.index
                trackIndex: index
                track: modelData
            }
        }

        Label {
            visible: card.expanded && card.chapters.length > 0
            text: (card.chaptersOpen ? Glyphs.open : Glyphs.closed) + "  " + qsTr("Chapters (%1)").arg(card.chapters.length)
            font.pixelSize: Metrics.fontCaption
            opacity: 0.75
            TapHandler { onTapped: card.chaptersOpen = !card.chaptersOpen }
        }
        Repeater {
            model: card.expanded && card.chaptersOpen ? card.chapters : []
            delegate: ChapterRow {
                required property var modelData
                chapter: modelData
            }
        }
    }
}

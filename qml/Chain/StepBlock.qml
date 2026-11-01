import QtQuick
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// One chain step: plain name, glyph, a mini-visual of what it does, its
// ffmpeg filter (or its time range), a bypass switch and × on hover. Badges
// call out clipping and other problems where they happen.
Rectangle {
    id: block
    objectName: "chainStep" + block.index

    required property int index
    required property string type
    required property string name
    required property string glyph
    required property string filter
    required property bool bypass
    required property var params
    required property bool hasRange
    required property real rangeStart
    required property real rangeEnd
    required property string warning
    required property bool available

    signal reorderRequested(real distance)

    readonly property bool selected: App.selection.kind === "step" && App.selection.index === block.index

    width: Metrics.stepWidth
    height: Metrics.stepHeight
    radius: Metrics.radiusLarge
    color: Theme.card
    border.width: block.selected ? 2 : 1
    border.color: block.selected ? Theme.accent : !block.available ? Theme.warn : Theme.blockBorder

    HoverHandler { id: blockHover }
    MouseArea {
        anchors.fill: parent
        property real pressX: 0
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        onPressed: mouse => { pressX = mouse.x; }
        onReleased: mouse => {
            if (Math.abs(mouse.x - pressX) > 24) block.reorderRequested(mouse.x - pressX);
            else App.selection.select("step", { index: block.index });
        }
    }

    Item {
        anchors.fill: parent
        opacity: block.bypass ? 0.4 : 1

        Row {
            x: 8
            y: 6
            spacing: Metrics.spacing
            Text { text: block.glyph; color: Theme.accentGlyph; font.pixelSize: Metrics.fontLarge }
            Text {
                text: block.name
                color: Theme.text
                font.pixelSize: Metrics.fontBody
                font.bold: true
                width: block.width - 52
                elide: Text.ElideRight
            }
        }
        MiniVisual {
            x: 8
            y: 27
            width: block.width - 16
            height: 24
            type: block.type
            params: block.params
            duration: App.project.duration
        }
        Text {
            x: 8
            y: 58
            width: block.width - 46
            elide: Text.ElideRight
            text: block.hasRange
                  ? Glyphs.range + " " + Format.timecode(block.rangeStart) + "–" + Format.timecode(block.rangeEnd)
                  : block.filter
            color: block.hasRange ? Theme.accentSoft : Theme.textDim
            font.pixelSize: Metrics.fontSmall
            font.family: Metrics.monoFamily
        }
    }

    BypassToggle {
        x: block.width - 34
        y: block.height - 20
        active: !block.bypass
        onToggled: App.project.setBypass(block.index, !block.bypass)
    }

    Text {
        visible: blockHover.hovered
        x: block.width - 18
        y: 4
        text: Glyphs.remove
        color: removeHover.hovered ? Theme.error : Theme.textMuted
        font.pixelSize: Metrics.fontTitle
        HoverHandler { id: removeHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: App.project.removeStep(block.index) }
    }

    Badge {
        visible: block.warning !== ""
        anchors { right: parent.right; rightMargin: 6 }
        y: -8
        text: block.warning
        tone: Theme.errorDeep
        textColor: Theme.textOnAccent
    }
}

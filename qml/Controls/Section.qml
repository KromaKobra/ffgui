import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// A collapsible group in a property sheet (ffprobe sections, Advanced options).
ColumnLayout {
    id: section

    property string title
    property bool expanded: true
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: Metrics.spacingSmall

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Metrics.rowHeight
        radius: Metrics.radius
        color: hover.hovered ? Theme.hover : Theme.card

        Label {
            anchors { left: parent.left; leftMargin: 6; right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            text: (section.expanded ? Glyphs.open : Glyphs.closed) + "  " + section.title
            font.bold: true
            font.pixelSize: Metrics.fontCaption
            elide: Text.ElideRight
        }
        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: section.expanded = !section.expanded }
    }

    ColumnLayout {
        id: body
        visible: section.expanded
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 10
        spacing: 3
    }
}

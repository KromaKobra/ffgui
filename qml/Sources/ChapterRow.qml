import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Theme

// A chapter under an input card. Clicking seeks there.
RowLayout {
    id: row

    required property var chapter

    Layout.fillWidth: true
    Layout.leftMargin: 14
    spacing: Metrics.spacingLarge

    Text {
        text: Format.timecode(row.chapter.start)
        color: Theme.textDim
        font.pixelSize: Metrics.fontSmall
        font.family: Metrics.monoFamily
    }
    Text {
        text: row.chapter.title || ""
        color: Theme.text
        font.pixelSize: Metrics.fontCaption
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: App.transport.seek(row.chapter.start) }
}

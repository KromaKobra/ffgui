import QtQuick
import FFgui.Theme

// The card thumbnail: cover art when the input has some, otherwise the
// glyph for its kind (file, folder, URL, device, generator).
Rectangle {
    id: tile

    property string cover            // image URL, or ""
    property string kind: "file"

    implicitWidth: 34
    implicitHeight: 34
    radius: Metrics.radiusMedium
    color: Theme.cardBorder
    clip: true

    Text {
        visible: art.status !== Image.Ready
        anchors.centerIn: parent
        text: Glyphs.inputKind(tile.kind)
        color: Theme.textMuted
        font.pixelSize: Metrics.fontBrand
    }
    Image {
        id: art
        anchors.fill: parent
        source: tile.cover
        sourceSize: Qt.size(68, 68)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }
}

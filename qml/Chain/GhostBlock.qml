import QtQuick
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// A converter ffmpeg inserts on its own, drawn as a dashed ghost block with
// the reason in its tooltip. `conversion`: { filter, from, to, reason }.
Item {
    id: ghost

    required property var conversion

    width: 124
    height: 64

    DashedRect { anchors.fill: parent; color: Theme.textDim }

    Column {
        anchors.centerIn: parent
        spacing: Metrics.spacingTight
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("auto-inserted")
            color: Theme.textDim
            font.pixelSize: Metrics.fontTiny
            font.italic: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: ghost.conversion.filter || ""
            color: Theme.text
            font.pixelSize: Metrics.fontCaption
            font.family: Metrics.monoFamily
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: ghost.conversion.from && ghost.conversion.to
                  ? Format.sampleRate(ghost.conversion.from.rate) + " " + Glyphs.arrow + " " + Format.sampleRate(ghost.conversion.to.rate)
                  : ""
            color: Theme.warn
            font.pixelSize: Metrics.fontSmall
        }
    }
    HoverHandler { id: ghostHover }
    TipLabel {
        visible: ghostHover.hovered
        text: qsTr("ffmpeg adds this converter itself: %1").arg(ghost.conversion.reason || "")
    }
}

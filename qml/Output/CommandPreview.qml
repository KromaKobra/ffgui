import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The exact ffmpeg command, one glance away (design principle 8). Hover for
// the multi-line form; Copy; expand into the Console.
RowLayout {
    spacing: Metrics.spacingLarge

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Metrics.controlHeight
        radius: Metrics.radiusMedium
        color: Theme.field
        border.color: Theme.divider

        Label {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            verticalAlignment: Text.AlignVCenter
            text: App.command.valid ? "$ " + App.command.text : App.command.placeholder
            font.family: Metrics.monoFamily
            font.pixelSize: Metrics.fontCaption
            color: App.command.valid ? Theme.command : Theme.textDim
            elide: Text.ElideRight
        }
        HoverHandler { id: hover }
        TipLabel { visible: hover.hovered && App.command.valid; delay: 700; text: App.command.multiline }
    }
    FfToolButton {
        text: qsTr("Copy")
        enabled: App.command.valid
        onClicked: App.copyText(App.command.text)
    }
    FfToolButton {
        text: Glyphs.expand
        tip: qsTr("Open in the Console (multi-line, editable)")
        onClicked: App.requestPanel("console")
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// What a panel shows before it has anything: one sentence and the action
// that fills it.
Item {
    id: empty

    property string glyph
    property string title
    property string text
    property string actionText
    property bool compact: false

    signal action()

    implicitHeight: column.implicitHeight + 24
    implicitWidth: column.implicitWidth + 24

    ColumnLayout {
        id: column
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 360)
        spacing: Metrics.spacingLarge

        Label {
            visible: empty.glyph !== "" && !empty.compact
            text: empty.glyph
            font.pixelSize: Metrics.fontHero
            opacity: 0.35
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            visible: empty.title !== ""
            text: empty.title
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            visible: empty.text !== ""
            text: empty.text
            font.pixelSize: Metrics.fontCaption
            opacity: 0.55
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        FfButton {
            visible: empty.actionText !== ""
            text: empty.actionText
            Layout.alignment: Qt.AlignHCenter
            onClicked: empty.action()
        }
    }
}

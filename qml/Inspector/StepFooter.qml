import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Controls
import FFgui.Theme

// Under a step's controls: its ffmpeg filter name with a docs link, and the
// exact filter text it produces (plain names first, ffmpeg names second).
Rectangle {
    id: footer

    property string filter        // "highpass + equalizer"
    property string primaryFilter // "highpass"
    property string filterText

    implicitHeight: column.implicitHeight + 16
    color: Theme.footer

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingLarge; leftMargin: Metrics.spacingWide }
        spacing: Metrics.spacingSmall

        Label {
            text: qsTr("ffmpeg filter <b>%1</b>").arg(footer.filter) + "   ·   "
                  + "<a href=\"https://ffmpeg.org/ffmpeg-filters.html#" + footer.primaryFilter + "\">" + qsTr("docs") + " ↗</a>"
            textFormat: Text.StyledText
            linkColor: Theme.accentSoft
            font.pixelSize: Metrics.fontCaption
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
        CommandText { text: footer.filterText }
    }
}

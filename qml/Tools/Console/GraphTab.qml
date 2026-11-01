pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The negotiated filter graph, as ffmpeg builds it: every filter with the
// format on its output, including converters ffmpeg inserts on its own.
ColumnLayout {
    id: tab

    spacing: Metrics.spacingSmall

    Hint {
        visible: App.project.graph.count === 0
        Layout.margins: Metrics.margin
        text: App.project.hasInput
              ? qsTr("The graph appears once ffmpeg has negotiated it (a quick dry run after each change).")
              : qsTr("Add an input to see the filter graph ffmpeg builds for it.")
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        topMargin: Metrics.spacing
        leftMargin: Metrics.spacingLarge
        model: App.project.graph
        delegate: Text {
            required property int index
            required property string kind
            required property string label
            required property string detail
            text: (index ? "  ↓  " : "     ") + label + "    " + detail
            color: kind === "auto" ? Theme.warn : Theme.textSoft
            font.family: Metrics.monoFamily
            font.pixelSize: Metrics.fontCaption
            padding: 2
        }
    }
}

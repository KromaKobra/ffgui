import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// One audio track of an input: codec, rate, depth, bitrate, layout icon,
// language and a star for the default track.
SelectableRow {
    id: row

    required property int inputIndex
    required property int trackIndex
    required property var track

    Layout.fillWidth: true
    selected: App.selection.kind === "track" && App.selection.index === row.inputIndex
              && App.selection.track === row.trackIndex
    onClicked: {
        App.project.currentInput = row.inputIndex;
        App.selection.select("track", { index: row.inputIndex, track: row.trackIndex });
    }

    RowLayout {
        anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
        spacing: Metrics.spacing

        Text { text: "#" + row.trackIndex; color: Theme.textDim; font.pixelSize: Metrics.fontSmall }
        Text {
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: Theme.text
            font.pixelSize: Metrics.fontCaption
            text: [row.track.codec,
                   row.track.rate ? Format.sampleRate(row.track.rate) : "",
                   row.track.depth ? row.track.depth + "-bit" : "",
                   row.track.bitrate ? Format.bitrate(row.track.bitrate) : ""].filter(s => s !== "").join("  ")
        }
        LayoutIcon { layout: row.track.layout || ""; channels: row.track.channels || 0 }
        Text { text: row.track.lang || "und"; color: Theme.textDim; font.pixelSize: Metrics.fontSmall }
        Text {
            text: Glyphs.star
            color: row.track.isDefault ? Theme.warn : Theme.borderStrong
            font.pixelSize: Metrics.fontCaption
        }
    }
}

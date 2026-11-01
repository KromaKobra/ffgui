pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Shown while files are dragged over the window. Zones depend on what is
// dragged: audio can be added or replace the selected input, or become an
// impulse response; images can become cover art.
DropArea {
    id: drop

    property var urls: []
    readonly property bool images: drop.urls.length > 0 && drop.urls.every(u => /\.(png|jpe?g|webp|bmp)$/i.test(u))
    readonly property var zones: {
        if (drop.images)
            return [{ id: "cover", title: qsTr("Use as cover art"), text: qsTr("Written to outputs whose metadata is copied or edited") }];
        const z = [{ id: "add", title: qsTr("Add as input"), text: qsTr("%n files", "", drop.urls.length) }];
        if (App.project.hasInput && drop.urls.length === 1)
            z.push({ id: "replace", title: qsTr("Replace input"), text: qsTr("Keep the chain and outputs, swap %1").arg(App.project.input.name) });
        if (App.project.hasInput && drop.urls.length === 1)
            z.push({ id: "ir", title: qsTr("Use as impulse response"), text: qsTr("Convolve the chain with this file") });
        return z;
    }
    property string hovered: "add"

    function zoneAt(x) {
        const n = drop.zones.length;
        return n ? drop.zones[Math.max(0, Math.min(n - 1, Math.floor(x / drop.width * n)))].id : "";
    }

    keys: ["text/uri-list"]
    onEntered: drag => {
        drop.urls = drag.urls.map(u => String(u));
        drop.hovered = drop.zoneAt(drag.x);
    }
    onPositionChanged: drag => drop.hovered = drop.zoneAt(drag.x)
    onExited: drop.urls = []
    onDropped: dropEvent => {
        const urls = dropEvent.urls;
        const zone = drop.zoneAt(dropEvent.x);
        drop.urls = [];
        if (zone === "add")
            App.project.addFiles(urls);
        else if (zone === "replace")
            App.project.replaceInput(App.project.currentInput, urls[0]);
        else if (zone === "ir")
            App.project.useImpulseResponse(urls[0]);
        else if (zone === "cover")
            App.project.setCoverArt(urls[0]);
        dropEvent.acceptProposedAction();
    }

    Rectangle {
        anchors.fill: parent
        visible: drop.containsDrag
        color: Theme.window
        opacity: 0.92

        RowLayout {
            anchors.fill: parent
            anchors.margins: 40
            spacing: 20
            Repeater {
                model: drop.zones
                delegate: Rectangle {
                    id: zone
                    required property var modelData
                    readonly property bool active: drop.hovered === modelData.id
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Metrics.radiusLarge * 2
                    color: zone.active ? Theme.selected : "transparent"
                    DashedRect { anchors.fill: parent; radius: parent.radius; color: zone.active ? Theme.accent : Theme.dashed; lineWidth: 2 }
                    Column {
                        anchors.centerIn: parent
                        spacing: Metrics.spacingLarge
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: zone.modelData.title
                            color: Theme.text
                            font.pixelSize: Metrics.fontHero
                            font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: zone.modelData.text
                            color: Theme.textDim
                            font.pixelSize: Metrics.fontBody
                        }
                    }
                }
            }
        }
    }
}

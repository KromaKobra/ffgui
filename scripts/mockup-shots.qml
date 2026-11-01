// Screenshots of the mock-up's workspaces, for comparing with ffgui's.
// scripts/screenshots.sh runs it:
//
//   qml6 -I <quilt> scripts/mockup-shots.qml -- file:///<quilt>/examples/ffgui/Main.qml <out dir>
import QtQuick

QtObject {
    id: shots

    readonly property var args: Qt.application.arguments
    readonly property string outDir: args[args.length - 1]
    readonly property var names: ["Default", "Convert", "Inspect", "Edit", "Master", "Restore", "Record & Stream", "Batch"]
    property var win
    property int next: 0

    property Timer timer: Timer {
        interval: 700
        onTriggered: {
            const name = shots.names[shots.next];
            const file = shots.outDir + "/mockup-" + name.toLowerCase().replace(/[^a-z]+/g, "-") + ".png";
            // The window's root item has no QML engine; grab its first child.
            shots.win.contentItem.children[0].grabToImage(result => {
                result.saveToFile(file);
                shots.next += 1;
                if (shots.next >= shots.names.length) {
                    Qt.exit(0);
                    return;
                }
                shots.win.switchWorkspace(shots.names[shots.next]);
                shots.timer.start();
            });
        }
    }

    Component.onCompleted: {
        const c = Qt.createComponent(args[args.length - 2]);
        if (c.status === Component.Error) {
            console.log(c.errorString());
            Qt.exit(1);
            return;
        }
        win = c.createObject(null, { width: 1600, height: 960 });
        Qt.callLater(() => {
            win.switchWorkspace("Default");
            timer.start();
        });
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend

// Console: the full command (editable, run as-is), the live log with a
// level filter, and the negotiated filter graph.
Item {
    id: panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: qsTr("Command"); width: implicitWidth }
            TabButton {
                text: App.errorCount > 0 ? qsTr("Log (%1, %2 errors)").arg(App.log.count).arg(App.errorCount)
                                         : qsTr("Log (%1)").arg(App.log.count)
                width: implicitWidth
            }
            TabButton { text: qsTr("Filter graph"); width: implicitWidth }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex
            CommandTab { }
            LogTab { }
            GraphTab { }
        }
    }
}

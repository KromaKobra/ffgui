import QtQuick
import FFgui.Theme

// A list row background that alternates with its neighbours; set `row` to
// the delegate's index.
Rectangle {
    property int row: 0
    color: row % 2 ? Theme.stripe : "transparent"
}

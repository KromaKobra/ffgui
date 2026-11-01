import QtQuick

// A tool button that opens `menu` below itself (or above, or right-aligned).
FfToolButton {
    id: control

    property FfMenu menu: null
    property bool alignRight: false
    property bool above: false

    onClicked: {
        if (!control.menu)
            return;
        control.menu.popup(control,
                           control.alignRight ? control.width - control.menu.width : 0,
                           control.above ? -control.menu.height : control.height);
    }
}

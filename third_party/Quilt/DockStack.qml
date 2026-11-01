pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// One tab group of a DockArea: a tab bar plus the content host that the
// group's panels are parented into (only the current one is visible). A
// panel that can't share a group (tabbable: false) gets a plain title bar
// instead of tabs.
Item {
    id: stack

    required property string stackId
    required property var area

    readonly property DockStyle style: area.style
    readonly property var manager: area.manager
    readonly property var info: area.stackInfo(stackId, area.revision)
    readonly property string currentId: info ? info.current : ""
    readonly property var currentPanel: manager ? manager.panel(currentId) : null
    readonly property int currentIndex: panelIds.indexOf(currentId)
    readonly property bool maximized: area.maximizedStackId === stackId
    readonly property bool solo: !!currentPanel && !currentPanel.tabbable && panelIds.length === 1
    readonly property var rect: area.geometry.rects[stackId]
    property alias contentHost: host

    // Only reassigned when the set or order of tabs changes, so switching
    // tabs never rebuilds them (and never kills a press in progress).
    property var panelIds: info ? info.panels : []
    onInfoChanged: {
        const ids = info ? info.panels : [];
        if (ids.join("\n") !== panelIds.join("\n"))
            panelIds = ids;
    }
    onCurrentIndexChanged: Qt.callLater(revealCurrent)

    visible: rect !== undefined
    x: rect ? rect.x : 0
    y: rect ? rect.y : 0
    width: rect ? rect.width : 0
    height: rect ? rect.height : 0
    Behavior on x { enabled: stack.area._animate; NumberAnimation { duration: stack.style.animationDuration; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: stack.area._animate; NumberAnimation { duration: stack.style.animationDuration; easing.type: Easing.OutCubic } }
    Behavior on width { enabled: stack.area._animate; NumberAnimation { duration: stack.style.animationDuration; easing.type: Easing.OutCubic } }
    Behavior on height { enabled: stack.area._animate; NumberAnimation { duration: stack.style.animationDuration; easing.type: Easing.OutCubic } }

    function select(panelId) {
        area.setCurrent(panelId);
    }

    // Insertion index for a drop at x (stack coordinates) over the tab bar.
    function tabIndexAt(x) {
        const cx = x - tabScroll.x + tabScroll.contentX;
        for (let i = 0; i < tabs.count; ++i) {
            const t = tabs.itemAt(i);
            if (t && cx < t.x + t.width / 2)
                return i;
        }
        return tabs.count;
    }

    // x (stack coordinates) of the insertion marker for `index`.
    function tabInsertX(index) {
        if (tabs.count === 0)
            return tabScroll.x;
        const last = index >= tabs.count;
        const t = tabs.itemAt(last ? tabs.count - 1 : Math.max(0, index));
        if (!t)
            return tabScroll.x;
        return tabScroll.x - tabScroll.contentX + (last ? t.x + t.width : t.x);
    }

    function revealCurrent() {
        const t = tabs.itemAt(currentIndex);
        if (!t)
            return;
        if (t.x < tabScroll.contentX)
            tabScroll.contentX = t.x;
        else if (t.x + t.width > tabScroll.contentX + tabScroll.width)
            tabScroll.contentX = t.x + t.width - tabScroll.width;
    }

    // Snapshot the content as the drag image, then run the platform drag.
    function beginTabDrag(p, handler) {
        if (!p || !p.movable || !manager || manager.dragPanel)
            return;
        const scale = Math.min(1, 240 / Math.max(1, host.width), 180 / Math.max(1, host.height));
        const size = Qt.size(Math.max(1, Math.round(host.width * scale)),
                             Math.max(1, Math.round(host.height * scale)));
        const started = host.grabToImage(result => {
            if (handler.active)
                runDrag(p, result.url);
        }, size);
        if (!started)
            runDrag(p, "");
    }

    function runDrag(p, imageUrl) {
        area.startDrag(p, imageUrl, Qt.point(12, 12));
        // The platform drag consumed the release; briefly disable the tab
        // drag handlers so they don't stay stuck in the active state.
        _resetDrag = true;
        _resetDrag = false;
    }

    property bool _resetDrag: false

    // ---------------------------------------------------------------------
    //  Background

    Rectangle {
        id: tabBarBackground
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: stack.style.tabHeight
        radius: stack.style.radius
        color: stack.style.tabBar
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: Math.min(parent.radius, parent.height)
            color: parent.color
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; top: tabBarBackground.bottom }
        radius: stack.style.radius
        color: stack.currentPanel ? stack.currentPanel.background : stack.style.panel
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: Math.min(parent.radius, parent.height)
            color: parent.color
        }
    }

    // ---------------------------------------------------------------------
    //  Tab bar

    Flickable {
        id: tabScroll
        anchors { left: parent.left; right: buttons.visible ? buttons.left : parent.right; top: parent.top; leftMargin: 2 }
        height: stack.style.tabHeight
        visible: !stack.solo
        contentWidth: tabRow.width
        contentHeight: height
        interactive: false
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        WheelHandler {
            onWheel: event => {
                const d = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x;
                tabScroll.contentX = Math.max(0, Math.min(tabScroll.contentWidth - tabScroll.width,
                                                          tabScroll.contentX - d / 2));
            }
        }

        Row {
            id: tabRow
            height: tabScroll.height

            Repeater {
                id: tabs
                model: stack.panelIds

                delegate: Item {
                    id: tab
                    required property string modelData
                    readonly property var panel: stack.manager ? stack.manager.panel(modelData) : null
                    readonly property bool current: stack.currentId === modelData
                    readonly property bool active: current && !!stack.manager && stack.manager.activePanel === panel
                    readonly property bool hasIcon: !!panel && panel.icon.toString() !== ""

                    width: Math.min(220, Math.max(56, label.implicitWidth + (hasIcon ? 20 : 0)
                                                   + (closeButton.visible ? 22 : 0) + 22))
                    height: tabRow.height

                    Rectangle {
                        anchors { fill: parent; topMargin: 3; leftMargin: 1; rightMargin: 1 }
                        radius: stack.style.radius - 1
                        color: tab.current ? (tab.panel ? tab.panel.background : stack.style.panel)
                                           : (tabHover.hovered ? stack.style.tabHover : "transparent")
                        Rectangle {
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                            height: parent.radius
                            color: parent.color
                            visible: tab.current
                        }
                    }

                    Rectangle {
                        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 3; leftMargin: 6; rightMargin: 6 }
                        height: 2
                        radius: 1
                        color: stack.style.accent
                        visible: tab.active
                    }

                    Image {
                        id: icon
                        visible: tab.hasIcon
                        source: tab.panel ? tab.panel.icon : ""
                        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter; verticalCenterOffset: 1 }
                        width: 14
                        height: 14
                        sourceSize: Qt.size(28, 28)
                        fillMode: Image.PreserveAspectFit
                    }

                    Text {
                        id: label
                        anchors {
                            left: tab.hasIcon ? icon.right : parent.left
                            leftMargin: tab.hasIcon ? 6 : 11
                            right: closeButton.visible ? closeButton.left : parent.right
                            rightMargin: closeButton.visible ? 2 : 11
                            verticalCenter: parent.verticalCenter
                            verticalCenterOffset: 1
                        }
                        text: tab.panel ? tab.panel.title : tab.modelData
                        elide: Text.ElideRight
                        color: tab.current ? stack.style.text : stack.style.textDim
                        font.pixelSize: stack.style.fontPixelSize
                    }

                    IconButton {
                        id: closeButton
                        style: stack.style
                        glyph: "close"
                        tip: "Close"
                        visible: !!tab.panel && tab.panel.closable && (tab.current || tabHover.hovered)
                        anchors { right: parent.right; rightMargin: 5; verticalCenter: parent.verticalCenter; verticalCenterOffset: 1 }
                        onClicked: tab.panel.close()
                    }

                    HoverHandler { id: tabHover }

                    TapHandler {
                        gesturePolicy: TapHandler.DragThreshold
                        onPressedChanged: if (pressed) stack.select(tab.modelData)
                        onDoubleTapped: stack.manager.toggleMaximized(tab.modelData)
                    }

                    TapHandler {
                        acceptedButtons: Qt.MiddleButton
                        onTapped: if (tab.panel && tab.panel.closable) tab.panel.close()
                    }

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: (eventPoint) => {
                            stack.select(tab.modelData);
                            tabMenu.panelId = tab.modelData;
                            tabMenu.popup(tab, eventPoint.position.x, eventPoint.position.y);
                        }
                    }

                    DragHandler {
                        id: tabDrag
                        target: null
                        enabled: !!tab.panel && tab.panel.movable && !stack._resetDrag
                        onActiveChanged: if (active) stack.beginTabDrag(tab.panel, tabDrag)
                    }
                }
            }
        }
    }

    // Title bar of a solo (non-tabbable) panel: same gestures as a tab.
    Item {
        id: titleBar
        anchors { left: parent.left; right: buttons.visible ? buttons.left : parent.right; top: parent.top }
        height: stack.style.tabHeight
        visible: stack.solo
        readonly property bool hasIcon: !!stack.currentPanel && stack.currentPanel.icon.toString() !== ""

        Image {
            id: titleIcon
            visible: titleBar.hasIcon
            source: stack.currentPanel ? stack.currentPanel.icon : ""
            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
            width: 14
            height: 14
            sourceSize: Qt.size(28, 28)
            fillMode: Image.PreserveAspectFit
        }

        Text {
            anchors {
                left: titleBar.hasIcon ? titleIcon.right : parent.left
                leftMargin: titleBar.hasIcon ? 6 : 10
                right: parent.right
                rightMargin: 6
                verticalCenter: parent.verticalCenter
            }
            text: stack.currentPanel ? stack.currentPanel.title : ""
            elide: Text.ElideRight
            color: stack.style.text
            font.pixelSize: stack.style.fontPixelSize
        }

        TapHandler {
            gesturePolicy: TapHandler.DragThreshold
            onPressedChanged: if (pressed) stack.select(stack.currentId)
            onDoubleTapped: stack.manager.toggleMaximized(stack.currentId)
        }

        TapHandler {
            acceptedButtons: Qt.MiddleButton
            onTapped: if (stack.currentPanel.closable) stack.currentPanel.close()
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: (eventPoint) => {
                tabMenu.panelId = stack.currentId;
                tabMenu.popup(titleBar, eventPoint.position.x, eventPoint.position.y);
            }
        }

        DragHandler {
            id: titleDrag
            target: null
            enabled: stack.solo && stack.currentPanel.movable && !stack._resetDrag
            onActiveChanged: if (active) stack.beginTabDrag(stack.currentPanel, titleDrag)
        }
    }

    Row {
        id: buttons
        anchors { right: parent.right; rightMargin: 4; top: parent.top; topMargin: 1 }
        height: stack.style.tabHeight
        spacing: 2
        visible: stack.width >= 150             // narrow stacks give the room to their tabs

        IconButton {
            style: stack.style
            glyph: "popout"
            tip: "Pop out into a new window"
            anchors.verticalCenter: parent.verticalCenter
            visible: !!stack.currentPanel && !!stack.manager && stack.manager.canFloat(stack.currentPanel)
                     && (stack.area.isMain || stack.area.panelCount > 1)
            onClicked: stack.manager.popOut(stack.currentId)
        }
        IconButton {
            style: stack.style
            glyph: stack.maximized ? "restore" : "maximize"
            tip: stack.maximized ? "Restore" : "Maximize"
            anchors.verticalCenter: parent.verticalCenter
            visible: stack.maximized || stack.area.stackCount > 1
            onClicked: stack.manager.toggleMaximized(stack.currentId)
        }
        IconButton {
            style: stack.style
            glyph: "close"
            tip: "Close"
            anchors.verticalCenter: parent.verticalCenter
            visible: stack.solo && stack.currentPanel.closable
            onClicked: stack.currentPanel.close()
        }
    }

    // ---------------------------------------------------------------------
    //  Content

    Item {
        id: host
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; top: tabBarBackground.bottom }
        clip: true
    }

    // Observes presses on the content to track the active panel. A
    // PointHandler only takes passive grabs, so the content still gets every
    // event (a TapHandler here would swallow clicks).
    Item {
        anchors.fill: host
        PointHandler {
            acceptedButtons: Qt.AllButtons
            onActiveChanged: {
                if (active && stack.currentPanel && stack.manager)
                    stack.manager.activePanel = stack.currentPanel;
            }
        }
    }

    Menu {
        id: tabMenu
        property string panelId
        readonly property var panel: stack.manager ? stack.manager.panel(panelId) : null

        MenuItem {
            text: "Pop Out"
            enabled: !!tabMenu.panel && !!stack.manager && stack.manager.canFloat(tabMenu.panel)
                     && (stack.area.isMain || stack.area.panelCount > 1)
            onTriggered: stack.manager.popOut(tabMenu.panelId)
        }
        MenuItem {
            text: stack.maximized ? "Restore" : "Maximize"
            enabled: stack.maximized || stack.area.stackCount > 1
            onTriggered: stack.manager.toggleMaximized(tabMenu.panelId)
        }
        MenuSeparator { }
        MenuItem {
            text: "Close"
            enabled: !!tabMenu.panel && tabMenu.panel.closable
            onTriggered: stack.manager.closePanel(tabMenu.panelId)
        }
    }

    component IconButton: Item {
        id: button
        required property DockStyle style
        property string glyph                  // close | popout | maximize | restore
        property string tip
        signal clicked()

        width: 20
        height: 20

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: button.style.text
            opacity: hover.hovered ? 0.12 : 0
        }

        Shape {
            anchors.centerIn: parent
            width: 12
            height: 12
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: hover.hovered ? button.style.text : button.style.textDim
                strokeWidth: 1.3
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg {
                    path: {
                        switch (button.glyph) {
                        case "close": return "M 2.5 2.5 L 9.5 9.5 M 9.5 2.5 L 2.5 9.5";
                        case "popout": return "M 5 2 H 2 V 10 H 10 V 7 M 7 2 H 10 V 5 M 10 2 L 5.5 6.5";
                        case "maximize": return "M 2 2 H 10 V 10 H 2 Z";
                        default: return "M 2 4.5 H 7.5 V 10 H 2 Z M 4.5 4.5 V 2 H 10 V 7.5 H 7.5";
                        }
                    }
                }
            }
        }

        HoverHandler { id: hover }
        TapHandler { onTapped: button.clicked() }
        ToolTip.visible: tip !== "" && hover.hovered
        ToolTip.text: tip
        ToolTip.delay: 700
    }
}

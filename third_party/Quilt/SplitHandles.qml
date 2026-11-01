pragma ComponentBehavior: Bound
import QtQuick

// Every split handle of a DockArea: hover feedback, cursors and dragging.
//
// One persistent item covers the whole area. Its containmentMask only
// accepts points over a handle, so everything else reaches the panels
// underneath. A drag here survives the tree being re-shaped at its start
// (Ctrl-drag isolating a segment), which per-handle items would not.
//
// Dragging moves the whole line the handle belongs to; with Ctrl held at the
// press only the segment under the cursor moves. While Ctrl is up the line
// snaps onto handles it is about to continue.
Item {
    id: handles

    required property var area

    property string hoverKey: ""
    property point hoverPos
    property bool hoverIndependent: false

    readonly property var hovered: hoverKey ? area.geometry.handles[hoverKey] : undefined
    readonly property bool dragHorizontal: area._resizeState ? area._resizeState.horizontal : true
    readonly property int cursor: (area.resizing ? dragHorizontal : (!hovered || hovered.horizontal))
                                  ? Qt.SplitHCursor : Qt.SplitVCursor

    // What to highlight: the line being dragged (plus the handle it snapped
    // to), or what a drag would move from the hovered spot.
    readonly property var lines: {
        const g = area.geometry;
        const r = area._resizeState;
        if (area.resizing && r) {
            const keys = r.snapKey ? r.keys.concat([r.snapKey]) : r.keys;
            return keys.map(k => g.handles[k]).filter(h => !!h);
        }
        return area.handleHighlight(hoverKey, hoverIndependent, hoverPos.x, hoverPos.y);
    }

    function updateHover() {
        const p = hover.point.position;
        hoverPos = p;
        hoverIndependent = (hover.point.modifiers & Qt.ControlModifier) !== 0;
        hoverKey = hover.hovered ? area.handleAt(p.x, p.y) : "";
    }

    containmentMask: QtObject {
        function contains(point: point): bool {
            return handles.area.handleAt(point.x, point.y) !== "";
        }
    }

    Repeater {
        model: handles.lines
        delegate: Rectangle {
            id: line
            required property var modelData
            readonly property bool vertical: modelData.horizontal   // a row's handle is a vertical bar
            x: vertical ? modelData.x + modelData.width / 2 - 1 : modelData.x
            y: line.vertical ? line.modelData.y : line.modelData.y + line.modelData.height / 2 - 1
            width: line.vertical ? 2 : line.modelData.width
            height: line.vertical ? line.modelData.height : 2
            radius: 1
            color: handles.area.style.accent
            opacity: handles.area.resizing ? 0.9 : 0.45
        }
    }

    HoverHandler {
        id: hover
        cursorShape: handles.cursor
        onPointChanged: handles.updateHover()
        onHoveredChanged: handles.updateHover()
    }

    DragHandler {
        id: drag
        target: null
        dragThreshold: 2
        cursorShape: handles.cursor
        onActiveChanged: {
            if (active) {
                const p = centroid.pressPosition;
                if (handles.area.beginResize(p.x, p.y, (centroid.modifiers & Qt.ControlModifier) !== 0))
                    handles.follow();           // the move that crossed the threshold counts too
            } else {
                handles.area.endResize();
            }
        }
        onCentroidChanged: if (active) handles.follow()
    }

    // This item never moves, so the pointer's offset from the press point is
    // the drag distance (activeTranslation skips the activating move).
    function follow() {
        const c = drag.centroid;
        area.resize(Qt.point(c.position.x - c.pressPosition.x, c.position.y - c.pressPosition.y),
                    (c.modifiers & Qt.ControlModifier) === 0);
    }
}

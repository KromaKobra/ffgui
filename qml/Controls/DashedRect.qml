import QtQuick
import QtQuick.Shapes
import FFgui.Theme

// A dashed outline: ghost blocks, drop zones.
Item {
    id: dashed

    property color color: Theme.dashed
    property real radius: 0
    property real lineWidth: 1

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: dashed.color
            strokeWidth: dashed.lineWidth
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3]
            fillColor: "transparent"
            PathRectangle {
                x: dashed.lineWidth / 2
                y: dashed.lineWidth / 2
                width: dashed.width - dashed.lineWidth
                height: dashed.height - dashed.lineWidth
                radius: dashed.radius
            }
        }
    }
}

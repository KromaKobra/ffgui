pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import FFgui.Backend

// The chain as a row: Input, then each step with the format on the wire
// before it, then any converter ffmpeg inserts, then Output.
Flickable {
    id: flick

    signal addRequested(Item anchor, int insertAt)

    readonly property string inputFormat: App.project.hasInput
        ? Format.signal({ rate: App.project.track.rate, fmt: App.project.track.sampleFmt, layout: App.project.track.layout })
        : ""
    readonly property int stepCount: App.project.steps.count
    readonly property string lastFormat: flick.stepCount > 0 ? (lastStep.values.formatOut || "") : flick.inputFormat

    ModelRow {
        id: lastStep
        model: App.project.steps
        row: flick.stepCount - 1
    }

    clip: true
    contentWidth: row.width + 24
    contentHeight: height
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.horizontal: ScrollBar { }

    Row {
        id: row
        x: 12
        y: Math.max(8, (flick.height - height) / 2)

        EndBlock { isInput: true }

        Repeater {
            model: App.project.steps
            delegate: Row {
                id: cell
                required property int index
                required property string formatIn
                required property bool formatChanged
                required property string type
                required property string name
                required property string glyph
                required property string filter
                required property bool bypass
                required property var params
                required property bool hasRange
                required property real rangeStart
                required property real rangeEnd
                required property string warning
                required property bool available

                Connector {
                    insertAt: cell.index
                    format: cell.index === 0 ? flick.inputFormat : cell.formatIn
                    changed: cell.formatChanged
                    onAddRequested: (anchor, at) => flick.addRequested(anchor, at)
                }
                StepBlock {
                    index: cell.index
                    type: cell.type
                    name: cell.name
                    glyph: cell.glyph
                    filter: cell.filter
                    bypass: cell.bypass
                    params: cell.params
                    hasRange: cell.hasRange
                    rangeStart: cell.rangeStart
                    rangeEnd: cell.rangeEnd
                    warning: cell.warning
                    available: cell.available
                    onReorderRequested: distance => App.project.moveStep(cell.index,
                        Math.max(0, Math.min(flick.stepCount - 1, cell.index + Math.round(distance / cell.width))))
                }
            }
        }

        Connector {
            insertAt: flick.stepCount
            format: flick.lastFormat
            onAddRequested: (anchor, at) => flick.addRequested(anchor, at)
        }

        Loader {
            active: !!App.project.converter.filter
            visible: active
            sourceComponent: Row {
                GhostBlock {
                    anchors.verticalCenter: parent.verticalCenter
                    conversion: App.project.converter
                }
                Connector {
                    format: App.project.converter.to ? Format.signal(App.project.converter.to) : ""
                    changed: true
                    showPlus: false
                }
            }
        }

        EndBlock { isInput: false }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The Media View's empty state, for the most common job: drop a file, choose
// a format, convert. Power features stay one click away but never block it.
Item {
    id: quick

    readonly property bool hasInput: App.project.inputs.count > 0
    readonly property bool hasOutput: App.project.outputs.count > 0
    // Side by side when there is room, stacked otherwise.
    readonly property bool wide: quick.width >= 830

    component Step: ColumnLayout {
        id: step
        property int number
        property string title
        property bool done
        default property alias content: body.data
        spacing: Metrics.spacingLarge
        Layout.preferredWidth: 220
        Layout.alignment: Qt.AlignTop
        RowLayout {
            spacing: Metrics.spacingLarge
            Rectangle {
                implicitWidth: 30
                implicitHeight: 30
                radius: 15
                color: step.done ? Theme.okDeep : Theme.accent
                Text {
                    anchors.centerIn: parent
                    text: step.done ? Glyphs.check : step.number
                    color: Theme.textOnAccent
                    font.pixelSize: Metrics.fontTitle
                    font.bold: true
                }
            }
            Label { text: step.title; font.pixelSize: Metrics.fontHero; font.bold: true }
        }
        ColumnLayout {
            id: body
            Layout.leftMargin: 38
            spacing: Metrics.spacing
        }
    }

    component Arrow: Label {
        visible: quick.wide
        text: Glyphs.arrow
        font.pixelSize: Metrics.fontHero
        opacity: 0.4
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: 4
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, steps.implicitHeight + 40)
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        GridLayout {
            id: steps
            anchors.centerIn: parent
            columns: quick.wide ? 5 : 1
            columnSpacing: 28
            rowSpacing: 20

            Step {
                number: 1
                title: qsTr("Drop a file")
                done: quick.hasInput
                Hint { text: qsTr("Drop audio anywhere in this window, or pick one."); Layout.preferredWidth: 200 }
                FfButton { text: qsTr("Add files…"); onClicked: App.requestDialog("addFiles") }
            }
            Arrow { }
            Step {
                number: 2
                title: qsTr("Choose a format")
                done: quick.hasOutput
                Repeater {
                    model: App.catalog.presets
                    delegate: FfButton {
                        required property string name
                        required property string description
                        text: name
                        flat: true
                        tip: description
                        Layout.alignment: Qt.AlignLeft
                        onClicked: App.project.addOutput(name)
                    }
                }
            }
            Arrow { }
            Step {
                number: 3
                title: qsTr("Convert")
                done: false
                Hint {
                    text: App.render.blocker !== "" ? App.render.blocker
                          : qsTr("A COPY badge in the output bar means no re-encode is needed.")
                    Layout.preferredWidth: 200
                }
                FfButton {
                    text: qsTr("Convert")
                    highlighted: true
                    enabled: App.render.blocker === ""
                    onClicked: App.render.start()
                }
            }
        }
    }
}

pragma Singleton
import QtQuick

// Sizes, spacing and type. Like Theme, the one place these numbers live.
QtObject {
    // Type
    readonly property string monoFamily: "monospace"
    readonly property int fontTiny: 9
    readonly property int fontSmall: 10
    readonly property int fontCaption: 11
    readonly property int fontBody: 12
    readonly property int fontLarge: 13
    readonly property int fontTitle: 15
    readonly property int fontBrand: 16
    readonly property int fontHero: 22

    // Corners
    readonly property real radiusSmall: 2
    readonly property real radius: 3
    readonly property real radiusMedium: 4
    readonly property real radiusLarge: 6

    // Spacing
    readonly property real spacingTight: 2
    readonly property real spacingSmall: 4
    readonly property real spacing: 6
    readonly property real spacingLarge: 8
    readonly property real spacingWide: 12
    readonly property real margin: 10

    // Fixed bars
    readonly property real headerHeight: 42
    readonly property real statusHeight: 26
    readonly property real transportHeight: 36
    readonly property real panelHeaderHeight: 30
    readonly property real rowHeight: 24
    readonly property real controlHeight: 26

    // Dock
    readonly property real dockGap: 3
    readonly property real tabHeight: 28

    // Property sheets
    readonly property real propKeyWidth: 112
    readonly property real formLabelWidth: 80

    // Media View
    readonly property real laneLabelWidth: 26
    readonly property real rulerHeight: 34
    readonly property real overviewHeight: 16

    // Chain
    readonly property real stepWidth: 156
    readonly property real stepHeight: 80
    readonly property real endBlockWidth: 150
    readonly property real connectorWidth: 132
}

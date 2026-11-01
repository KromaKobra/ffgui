pragma ComponentBehavior: Bound
import QtQuick
import Quilt
import FFgui.Backend
import FFgui.Theme
import FFgui.Sources
import FFgui.Media
import FFgui.Meters
import FFgui.Chain
import FFgui.Inspector
import FFgui.Tools.Analysis
import FFgui.Tools.Scopes
import FFgui.Tools.Metadata
import FFgui.Tools.Routing
import FFgui.Tools.Compare
import FFgui.Tools.Console
import FFgui.Tools.Queue
import FFgui.Tools.Presets
import FFgui.Tools.Record
import FFgui.Tools.Generate
import FFgui.Tools.StreamOut
import FFgui.Tools.Transcript
import FFgui.Tools.Capabilities
import "PanelRegistry.js" as Registry
import "Workspaces.js" as Workspaces

// The docking area: one DockPanel per PanelRegistry entry. With
// WorkspaceController and PanelsMenu this is the only code that touches
// Quilt, and it uses only Quilt's public API (DockArea, DockPanel,
// DockStyle, DockWindow, Dock). Panel content is loaded while the panel is
// open; panels never talk to the dock themselves, they call
// App.requestPanel().
DockArea {
    id: dock

    // Pop-out windows, so a requested pop-out can be given a usable size.
    property var popouts: []

    function zone(area) {
        return area === "left" ? Dock.Left : area === "right" ? Dock.Right
             : area === "bottom" ? Dock.Bottom : area === "top" ? Dock.Top : Dock.Center;
    }

    function showPanel(panelId, popOut) {
        const p = dock.panel(panelId);
        if (!p)
            return;
        if (!popOut || p.isOpen) {
            dock.openPanel(panelId);
            return;
        }
        // A closed panel has no size to carry over; give its window room.
        const area = dock.popOut(panelId);
        const win = dock.popouts.find(w => w.dockArea === area);
        if (win) {
            win.width = Math.max(win.width, 640);
            win.height = Math.max(win.height, 460);
        }
    }

    defaultLayout: Workspaces.layout("Default").main

    // ffgui's theme, in Quilt's terms.
    style: DockStyle {
        background: Theme.window
        panel: Theme.panel
        tabBar: Theme.chrome
        tabHover: Theme.card
        text: Theme.text
        textDim: Theme.textDim
        accent: Theme.accent
        gap: Metrics.dockGap
        tabHeight: Metrics.tabHeight
        fontPixelSize: Metrics.fontBody
    }

    windowComponent: Component {
        DockWindow {
            id: popout
            title: "ffgui — " + popout.dockArea.title
            Component.onCompleted: dock.popouts = dock.popouts.concat([popout])
            Component.onDestruction: dock.popouts = dock.popouts.filter(w => w !== popout)
        }
    }

    // DockArea's default property only takes panels, hence a property.
    readonly property Connections requests: Connections {
        target: App
        function onPanelRequested(panelId, popOut) { dock.showPanel(panelId, popOut); }
    }

    // A dock panel from its registry entry, with lazily loaded content.
    component ShellPanel: DockPanel {
        id: shellPanel
        required property string registryId
        property Component content
        readonly property var spec: Registry.panel(shellPanel.registryId)

        panelId: shellPanel.registryId
        title: shellPanel.spec.title
        area: dock.zone(shellPanel.spec.area)
        areaSize: shellPanel.spec.areaSize || 0.25
        openOnStart: !!shellPanel.spec.core
        closable: !shellPanel.spec.core
        movable: shellPanel.spec.movable !== false
        tabbable: shellPanel.spec.tabbable !== false
        floatable: shellPanel.spec.floatable !== false
        minimumWidth: shellPanel.spec.minimumWidth || 120
        minimumHeight: shellPanel.spec.minimumHeight || 80
        maximumWidth: shellPanel.spec.maximumWidth || Number.POSITIVE_INFINITY

        Loader {
            anchors.fill: parent
            active: shellPanel.isOpen
            sourceComponent: shellPanel.content
        }
    }

    // ----- Core regions, always present
    ShellPanel { registryId: "sources"; content: Component { SourcesPanel { } } }
    ShellPanel { registryId: "media"; content: Component { MediaView { } } }
    ShellPanel { registryId: "meters"; content: Component { MetersStrip { } } }
    ShellPanel { registryId: "chain"; content: Component { ChainPanel { } } }
    ShellPanel { registryId: "inspector"; content: Component { InspectorPanel { } } }

    // ----- Toggleable panels, closed until opened
    ShellPanel { registryId: "analysis"; content: Component { AnalysisPanel { } } }
    ShellPanel { registryId: "scopes"; content: Component { ScopesPanel { } } }
    ShellPanel { registryId: "metadata"; content: Component { MetadataPanel { } } }
    ShellPanel { registryId: "routing"; content: Component { RoutingPanel { } } }
    ShellPanel { registryId: "compare"; content: Component { ComparePanel { } } }
    ShellPanel { registryId: "console"; content: Component { ConsolePanel { } } }
    ShellPanel { registryId: "queue"; content: Component { QueuePanel { } } }
    ShellPanel { registryId: "presets"; content: Component { PresetsPanel { } } }
    ShellPanel { registryId: "record"; content: Component { RecordPanel { } } }
    ShellPanel { registryId: "generate"; content: Component { GeneratePanel { } } }
    ShellPanel { registryId: "streamout"; content: Component { StreamOutPanel { } } }
    ShellPanel { registryId: "transcript"; content: Component { TranscriptPanel { } } }
    ShellPanel { registryId: "capabilities"; content: Component { CapabilitiesPanel { } } }
}

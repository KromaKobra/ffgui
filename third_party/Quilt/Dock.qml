import QtQml

// Enumerations used by Quilt's public API, e.g. `area: Dock.Left`.
QtObject {
    // Where a panel goes when nothing else (a saved or default layout, or the
    // place it was closed from) decides. See DockPanel.area.
    enum Area { Center, Left, Right, Top, Bottom }

    // What closing a pop-out window does with its panels.
    // See DockArea.windowClosePolicy.
    enum ClosePolicy { ReturnPanels, ClosePanels }
}

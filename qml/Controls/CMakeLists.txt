# FFgui.Controls: the Ff* input facade (the swap point for Quilt's own
# controls) and shared patterns. Imports only FFgui.Theme: no Backend, so it
# stays reusable and could move upstream into Quilt.
ffgui_add_qml_module(ffgui_controls
    URI FFgui.Controls
    QML_FILES
        Badge.qml
        ButtonRow.qml
        BypassToggle.qml
        Caption.qml
        Card.qml
        CheckableListMenu.qml
        CommandText.qml
        CorrelationMeter.qml
        DashedRect.qml
        Divider.qml
        EmptyState.qml
        FfButton.qml
        FfCheckBox.qml
        FfComboBox.qml
        FfItemDelegate.qml
        FfMenuItem.qml
        FfMenu.qml
        FfMenuSeparator.qml
        FfRadioButton.qml
        FfSlider.qml
        FfSpinBox.qml
        FfSwitch.qml
        FfTextArea.qml
        FfTextField.qml
        FfToolButton.qml
        FormRow.qml
        Hint.qml
        LevelMeter.qml
        MenuButton.qml
        NumberField.qml
        PanelToolbar.qml
        ParamControl.qml
        Pill.qml
        ProgressStrip.qml
        Prop.qml
        Section.qml
        SegmentedControl.qml
        SelectableRow.qml
        StageBadge.qml
        StatusDot.qml
        StripedRow.qml
        TimecodeRow.qml
        TimeField.qml
        TipLabel.qml
    IMPORTS FFgui.Theme
)

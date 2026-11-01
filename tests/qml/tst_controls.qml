import QtQuick
import QtTest
import FFgui.Controls

// The FFgui.Controls components on their own: they load, and their own
// behaviour (parsing, clamping, signalling) works.
Item {
    id: root
    width: 600
    height: 400

    Component { id: segmented; SegmentedControl { options: ["a", { value: 2, label: "Two" }] } }
    Component { id: timeField; TimeField { } }
    Component { id: numberField; NumberField { from: -10; to: 10; decimals: 1 } }
    Component { id: section; Section { title: "Group"; Prop { key: "k"; value: "v" } } }
    Component { id: param; ParamControl { } }
    Component { id: listMenu; CheckableListMenu { } }
    Component { id: meter; LevelMeter { height: 200 } }

    TestCase {
        name: "Controls"
        when: windowShown

        function init() {
            failOnWarning(/.*/);
        }

        function test_every_component_loads_data() {
            return ["Badge", "BypassToggle", "ButtonRow", "Caption", "Card", "CheckableListMenu", "CommandText",
                    "CorrelationMeter", "DashedRect", "Divider", "EmptyState", "FfButton", "FfCheckBox", "FfComboBox",
                    "FfItemDelegate", "FfMenu", "FfMenuItem", "FfMenuSeparator", "FfRadioButton", "FfSlider", "FfSpinBox",
                    "FfSwitch", "FfTextArea", "FfTextField", "FfToolButton", "FormRow", "Hint", "LevelMeter", "MenuButton",
                    "NumberField", "PanelToolbar", "Pill", "ProgressStrip", "Prop", "Section", "SegmentedControl",
                    "SelectableRow", "StageBadge", "StatusDot", "StripedRow", "TimeField", "TimecodeRow"]
                .map(name => ({ tag: name, name: name }));
        }
        function test_every_component_loads(data) {
            const obj = Qt.createQmlObject("import QtQuick\nimport FFgui.Controls\n" + data.name + " { }", root);
            verify(obj, data.name);
            obj.destroy();
        }

        function test_segmented_control_reports_values() {
            const c = createTemporaryObject(segmented, root);
            const spy = createTemporaryObject(signalSpyComponent, root, { target: c, signalName: "activated" });
            c.current = 2;
            const buttons = c.children.filter(b => b.text !== undefined);
            compare(buttons.length, 2);
            verify(!buttons[0].checked);
            verify(buttons[1].checked);
            mouseClick(buttons[0]);
            compare(spy.count, 1);
            compare(spy.signalArguments[0][0], "a");
        }

        function test_time_field_parses_and_reverts() {
            const f = createTemporaryObject(timeField, root, { seconds: 5, display: "00:05" });
            const spy = createTemporaryObject(signalSpyComponent, root, { target: f, signalName: "edited" });
            f.text = "1:02.5";
            f.editingFinished();
            compare(spy.count, 1);
            compare(spy.signalArguments[0][0], 62.5);
            f.text = "nonsense";
            f.editingFinished();
            compare(spy.count, 1);
            compare(f.text, "00:05");
            compare(f.parse("1:02:03"), 3723);
            verify(isNaN(f.parse("1:75")));
        }

        function test_number_field_clamps_and_takes_a_real_minus() {
            const f = createTemporaryObject(numberField, root, { value: 1 });
            const spy = createTemporaryObject(signalSpyComponent, root, { target: f, signalName: "edited" });
            compare(f.text, "1.0");
            f.text = "−3.25";
            f.editingFinished();
            compare(spy.signalArguments[0][0], -3.3);       // decimals: 1
            f.text = "99";
            f.editingFinished();
            compare(spy.signalArguments[1][0], 10);
            f.value = -2;
            compare(f.text, "−2.0");
        }

        function test_section_collapses() {
            const s = createTemporaryObject(section, root);
            verify(s.expanded);
            s.expanded = false;
            verify(!s.children[1].visible);
        }

        function test_param_control_by_type_data() {
            return [
                { tag: "number", option: { key: "g", label: "Gain", unit: "dB", type: "number", min: -12, max: 12, step: 0.5, "default": 0 }, value: 3 },
                { tag: "choice", option: { key: "t", label: "Type", type: "choice", choices: ["in", "out"], "default": "in" }, value: "out" },
                { tag: "bool", option: { key: "b", label: "On", type: "bool", "default": false }, value: true },
                { tag: "string", option: { key: "s", label: "Text", type: "string", "default": "" }, value: "x" }
            ];
        }
        function test_param_control_by_type(data) {
            const p = createTemporaryObject(param, root, { option: data.option, value: data.value, width: 300 });
            verify(p);
            verify(!p.isDefault);
            const spy = createTemporaryObject(signalSpyComponent, root, { target: p, signalName: "edited" });
            // The reset button sends the default back.
            mouseClick(findChild(p, "reset"));
            compare(spy.count, 1);
            compare(spy.signalArguments[0][0], data.option["default"]);
        }

        function test_checkable_list_menu_reports_toggles() {
            const m = createTemporaryObject(listMenu, root, {
                items: [{ key: "a", label: "A" }, { key: "b", label: "B" }],
                isChecked: key => key === "b"
            });
            compare(m.count, 2);
            verify(!m.itemAt(0).checked);
            verify(m.itemAt(1).checked);
            const spy = createTemporaryObject(signalSpyComponent, root, { target: m, signalName: "toggled" });
            m.itemAt(0).triggered();
            compare(spy.count, 1);
            compare(spy.signalArguments[0][0], "a");
        }

        function test_level_meter_maps_db() {
            const m = createTemporaryObject(meter, root);
            compare(m.frac(0), 1);
            compare(m.frac(-60), 0);
            compare(m.frac(-30), 0.5);
            compare(m.frac(-90), 0);
        }
    }

    Component { id: signalSpyComponent; SignalSpy { } }
}

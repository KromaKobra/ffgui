import QtQuick
import FFgui.Backend
import "PanelRegistry.js" as Registry

// Everything the command palette can do: open panels, switch workspaces, add
// chain steps, toggle layers, and the project commands. Each action has a
// plain title, an ffmpeg name or detail as its subtitle, and keywords, so
// "noise" and "afftdn" both find "Add step: Remove noise".
QtObject {
    id: registry

    required property WorkspaceController workspaces

    readonly property var actions: {
        const list = [];
        const add = (category, title, subtitle, keywords, run) =>
            list.push({ category: category, title: title, subtitle: subtitle || "", keywords: keywords || [], run: run });

        add(qsTr("Input"), qsTr("Add files…"), "ffprobe", ["open", "import", "load"], () => App.requestDialog("addFiles"));
        add(qsTr("Input"), qsTr("Add folder (batch)…"), "", ["batch", "directory"], () => App.requestDialog("addFolder"));
        add(qsTr("Input"), qsTr("Add URL…"), "", ["stream", "http"], () => App.requestDialog("addUrl"));
        add(qsTr("Input"), qsTr("Record from device"), "-f pulse", ["microphone", "capture"], () => App.requestPanel("record"));
        add(qsTr("Input"), qsTr("Generate signal"), "lavfi", ["tone", "sine", "noise"], () => App.requestPanel("generate"));

        add(qsTr("Deliver"), qsTr("Render"), "Ctrl+Enter", ["export", "convert", "encode"], () => App.render.start("all"));
        add(qsTr("Deliver"), qsTr("Render selection only"), "", ["export range"], () => App.render.start("selection"));
        add(qsTr("Deliver"), qsTr("Add to queue"), "", ["batch"], () => App.render.enqueue());
        add(qsTr("Deliver"), qsTr("Copy the ffmpeg command"), "", ["clipboard", "cli"], () => App.copyText(App.command.text));

        add(qsTr("Tools"), qsTr("FFmpeg tool settings…"), "", ["paths", "configuration"], () => App.requestDialog("settings"));
        add(qsTr("Tools"), qsTr("Refresh capture devices"), "-sources", ["microphone", "record"], () => App.capture.refreshDevices());
        add(qsTr("Tools"), qsTr("Compare original and processed"), "asdr / apsnr", ["difference", "checksum"], () => { App.requestPanel("compare"); App.compare.run(); });
        add(qsTr("Tools"), qsTr("Import transcript…"), "SRT / TXT / JSON", ["speech", "subtitles"], () => App.requestDialog("importTranscript"));
        add(qsTr("Tools"), qsTr("Stop recording"), "", ["record", "capture"], () => App.capture.stop());
        add(qsTr("Tools"), qsTr("Stop all streams"), "", ["broadcast", "stream"], () => App.stream.stopAll());
        add(qsTr("Project"), qsTr("New project"), "", [], () => App.project.newProject());
        add(qsTr("Project"), qsTr("Open project…"), "Ctrl+O", [".ffgui", "recipe"], () => App.requestDialog("openProject"));
        add(qsTr("Project"), qsTr("Save project"), "Ctrl+S", [".ffgui", "recipe"], () => App.requestDialog("saveProject"));
        add(qsTr("Project"), qsTr("Undo"), "Ctrl+Z", [], () => App.undo.undo());
        add(qsTr("Project"), qsTr("Redo"), "Ctrl+Shift+Z", [], () => App.undo.redo());

        add(qsTr("Playback"), qsTr("Play / pause"), "Space", ["ffplay"], () => App.transport.toggle());
        add(qsTr("Playback"), qsTr("Open in ffplay"), "ffplay", ["reference", "external player"], () => App.openInFfplay());

        for (const p of Registry.panels)
            add(qsTr("Panel"), qsTr("Show %1").arg(p.title.replace(/^\d · /, "")), p.description, p.keywords,
                () => App.requestPanel(p.id));

        for (const name of registry.workspaces.names)
            add(qsTr("Workspace"), qsTr("Workspace: %1").arg(name), registry.workspaces.descriptions[name], ["layout"],
                () => registry.workspaces.switchTo(name));
        add(qsTr("Workspace"), qsTr("Reset this workspace"), "", ["layout"], () => registry.workspaces.reset());

        for (const layer of App.view.layerList)
            add(qsTr("Layer"), qsTr("Toggle layer: %1").arg(layer.label), "", ["media view", "overlay"],
                () => App.view.setLayer(layer.key, !App.view.layer(layer.key)));

        const catalogRevision = App.catalog.steps.revision;
        for (let i = 0; catalogRevision >= 0 && i < App.catalog.steps.count; ++i) {
            const s = App.catalog.steps.get(i);
            if (!s.available) continue;
            add(qsTr("Add step"), qsTr("Add step: %1").arg(s.name), s.filter, s.keywords.concat([s.category, s.blurb]),
                () => App.project.addStep(s.key));
        }
        return list;
    }

    // The actions matching every word of `query`, best first.
    function search(query) {
        const words = query.toLowerCase().split(/\s+/).filter(w => w !== "");
        if (words.length === 0)
            return registry.actions.slice(0, 40);
        const scored = [];
        for (const a of registry.actions) {
            const title = a.title.toLowerCase();
            const hay = [title, a.subtitle.toLowerCase(), a.category.toLowerCase()].concat(a.keywords.map(k => String(k).toLowerCase()));
            if (!words.every(w => hay.some(h => h.indexOf(w) >= 0)))
                continue;
            let score = 0;
            for (const w of words)
                score += title.indexOf(w) >= 0 ? (title.indexOf(w) === 0 || title.indexOf(" " + w) >= 0 ? 3 : 2) : 1;
            scored.push({ action: a, score: score });
        }
        scored.sort((x, y) => y.score - x.score);
        return scored.slice(0, 40).map(s => s.action);
    }
}

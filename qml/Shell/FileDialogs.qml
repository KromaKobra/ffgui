pragma ComponentBehavior: Bound
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls

// The file, folder and URL dialogs panels ask for with App.requestDialog().
Item {
    id: dialogs

    readonly property var audioFilters: [
        qsTr("Audio and video (*.wav *.flac *.mp3 *.m4a *.aac *.ogg *.opus *.mka *.mkv *.mp4 *.mov *.aiff *.aif *.wma *.ac3 *.dts *.caf *.w64 *.ape *.wv)"),
        qsTr("All files (*)")
    ]
    property var pending: ({})       // options of the dialog that is open

    Connections {
        target: App
        function onDialogRequested(dialogId, options) {
            dialogs.pending = options;
            switch (dialogId) {
            case "addFiles": addFiles.open(); break;
            case "addFolder": addFolder.open(); break;
            case "addUrl": addUrl.open(); break;
            case "openProject": openProject.open(); break;
            case "saveProject": saveProject.open(); break;
            case "exportAnalysis":
                exportAnalysis.nameFilters = options.format === "csv" ? [qsTr("CSV (*.csv)")] : [qsTr("JSON (*.json)")];
                exportAnalysis.defaultSuffix = options.format === "csv" ? "csv" : "json";
                exportAnalysis.open();
                break;
            case "outputFolder": outputFolder.open(); break;
            case "savePreset": savePreset.open(); break;
            case "importPresets": importPresets.open(); break;
            case "exportPresets": exportPresets.open(); break;
            case "replaceCover": replaceCover.open(); break;
            default:
                App.toast(qsTr("That isn't available yet"));
            }
        }
    }

    FileDialog {
        id: addFiles
        title: qsTr("Add audio files")
        fileMode: FileDialog.OpenFiles
        nameFilters: dialogs.audioFilters
        currentFolder: StandardPaths.writableLocation(StandardPaths.MusicLocation)
        onAccepted: App.project.addFiles(selectedFiles)
    }
    FolderDialog {
        id: addFolder
        title: qsTr("Add a folder of audio files")
        currentFolder: StandardPaths.writableLocation(StandardPaths.MusicLocation)
        onAccepted: App.project.addFolder(selectedFolder)
    }
    FileDialog {
        id: openProject
        title: qsTr("Open project")
        nameFilters: [qsTr("ffgui projects (*.ffgui)")]
        onAccepted: App.project.open(selectedFile)
    }
    FileDialog {
        id: saveProject
        title: qsTr("Save project")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "ffgui"
        nameFilters: [qsTr("ffgui projects (*.ffgui)")]
        onAccepted: App.project.save(selectedFile)
    }
    FileDialog {
        id: exportAnalysis
        title: qsTr("Export analysis")
        fileMode: FileDialog.SaveFile
        onAccepted: {
            const ok = dialogs.pending.format === "csv" ? App.analysis.exportCsv(selectedFile) : App.analysis.exportJson(selectedFile);
            App.toast(ok ? qsTr("Analysis exported") : qsTr("Couldn't write the file"), ok ? "success" : "error");
        }
    }
    FolderDialog {
        id: outputFolder
        title: qsTr("Folder for this output")
        onAccepted: App.project.setOutput(dialogs.pending.index, { folder: selectedFolder.toString() })
    }
    FileDialog {
        id: replaceCover
        title: qsTr("Cover art")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp)")]
        onAccepted: App.project.setCoverArt(selectedFile)
    }

    FileDialog {
        id: importPresets
        title: qsTr("Import presets")
        nameFilters: [qsTr("Presets (*.json)")]
        onAccepted: App.toast(App.catalog.importPresets(selectedFile) ? qsTr("Presets imported") : qsTr("Couldn't import presets"))
    }
    FileDialog {
        id: exportPresets
        title: qsTr("Export presets")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("Presets (*.json)")]
        onAccepted: App.toast(App.catalog.exportPresets(selectedFile) ? qsTr("Presets exported") : qsTr("Couldn't export presets"))
    }
    Dialog {
        id: savePreset
        title: qsTr("Save current preset")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: App.toast(App.project.savePreset(presetName.text, presetKind.currentIndex === 1) ? qsTr("Preset saved") : qsTr("Add a name and a chain or output first"))
        ColumnLayout {
            FfTextField { id: presetName; placeholderText: qsTr("Preset name"); Layout.fillWidth: true }
            FfComboBox { id: presetKind; model: [qsTr("Current output"), qsTr("Processing chain")]; Layout.fillWidth: true }
        }
    }

    Dialog {
        id: addUrl
        title: qsTr("Add a URL")
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 460
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: url.forceActiveFocus()
        onAccepted: {
            if (url.text.trim() !== "")
                App.project.addUrl(url.text.trim());
            url.text = "";
        }
        ColumnLayout {
            anchors.fill: parent
            Hint { text: qsTr("Anything ffmpeg can open: http(s), HLS (.m3u8), RTMP, SRT, Icecast…") }
            FfTextField {
                id: url
                Layout.fillWidth: true
                mono: true
                placeholderText: "https://…"
                onAccepted: addUrl.accept()
            }
        }
    }
}

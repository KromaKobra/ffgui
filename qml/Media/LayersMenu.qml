import QtQuick
import FFgui.Backend
import FFgui.Controls

// The Media View's layer toggles (waveform, spectrogram, analysis overlays).
CheckableListMenu {
    width: 240
    items: App.view.layerList
    isChecked: key => App.view.layers[key] === true
    onToggled: (key, on) => App.view.setLayer(key, on)
}

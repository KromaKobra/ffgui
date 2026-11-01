pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Backend
import FFgui.Theme

// The timeline ruler over the visible window, in timecode, seconds, samples
// or bars at a BPM, with chapter flags hanging from it.
Item {
    id: ruler

    property real viewStart: 0
    property real viewEnd: 1
    property string unit: "timecode"
    property int sampleRate: 48000
    property real bpm: 120
    property var chapters: []
    property bool showChapters: true

    readonly property real span: Math.max(1e-6, ruler.viewEnd - ruler.viewStart)
    // Seconds between ticks: the first step that leaves ~90 px apart.
    readonly property real step: {
        const steps = [0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60,
                       120, 300, 600, 900, 1800, 3600, 7200];
        for (const s of steps)
            if (s / ruler.span * ruler.width >= 90)
                return s;
        return 7200;
    }
    readonly property real firstTick: Math.ceil(ruler.viewStart / ruler.step) * ruler.step

    function xOf(t) { return (t - ruler.viewStart) / ruler.span * ruler.width; }

    function label(t) {
        if (ruler.unit === "seconds")
            return Format.number(t, ruler.step < 0.01 ? 3 : ruler.step < 1 ? 2 : 0) + " s";
        if (ruler.unit === "samples")
            return String(Math.round(t * ruler.sampleRate));
        if (ruler.unit === "bars") {
            const beat = 60 / ruler.bpm;
            const beats = Math.floor(t / beat + 1e-6);
            return (Math.floor(beats / 4) + 1) + "." + (beats % 4 + 1);
        }
        return Format.timecode(t, ruler.step < 1);
    }

    clip: true

    Repeater {
        model: Math.max(0, Math.floor((ruler.viewEnd - ruler.firstTick) / ruler.step) + 1)
        delegate: Item {
            id: tick
            required property int index
            readonly property real t: ruler.firstTick + index * ruler.step
            x: ruler.xOf(t)
            Rectangle { y: 12; width: 1; height: 6; color: Theme.textFaint }
            Text {
                x: 3
                y: 1
                text: ruler.label(tick.t)
                color: Theme.textDim
                font.pixelSize: Metrics.fontSmall
            }
        }
    }

    Repeater {
        model: ruler.showChapters ? ruler.chapters : []
        delegate: ChapterFlag {
            required property int index
            required property var modelData
            readonly property real next: index + 1 < ruler.chapters.length ? ruler.chapters[index + 1].start : ruler.viewEnd
            visible: modelData.start <= ruler.viewEnd && next >= ruler.viewStart
            x: ruler.xOf(modelData.start)
            y: 18
            title: modelData.title || ""
            room: ruler.xOf(next) - x - 3
        }
    }
}

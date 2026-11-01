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
    readonly property real step: ruler.stepFor(ruler.span, ruler.width)
    // The tick times. Built in one go, with the step computed for exactly this
    // span and width, and never more than kMaxTicks: a count derived from
    // separately updated bindings can briefly pair a long span with a tiny
    // step and ask for millions of delegates.
    readonly property var ticks: {
        const w = ruler.width, span = ruler.span;
        if (!(w > 0) || !(span > 0))
            return [];
        const step = ruler.stepFor(span, w);
        const out = [];
        for (let t = Math.ceil(ruler.viewStart / step) * step; t <= ruler.viewEnd + 1e-9 && out.length < ruler.kMaxTicks; t += step)
            out.push(t);
        return out;
    }
    readonly property int kMaxTicks: 200

    function stepFor(span, width) {
        const steps = [0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60,
                       120, 300, 600, 900, 1800, 3600, 7200, 14400, 28800, 86400];
        for (const s of steps)
            if (s / span * width >= 90)
                return s;
        return 86400;
    }

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
        model: ruler.ticks
        delegate: Item {
            id: tick
            required property real modelData
            readonly property real t: modelData
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

    // Flags for the chapters that start in view (at most 100), each knowing
    // where the next one starts so labels never overlap.
    readonly property var visibleChapters: {
        if (!ruler.showChapters)
            return [];
        const out = [];
        const list = ruler.chapters;
        for (let i = 0; i < list.length && out.length < 100; ++i) {
            const next = i + 1 < list.length ? list[i + 1].start : Infinity;
            if (list[i].start <= ruler.viewEnd && next >= ruler.viewStart)
                out.push({ start: list[i].start, title: list[i].title || "", next: Math.min(next, ruler.viewEnd) });
        }
        return out;
    }
    Repeater {
        model: ruler.visibleChapters
        delegate: ChapterFlag {
            required property var modelData
            readonly property real next: modelData.next
            x: ruler.xOf(modelData.start)
            y: 18
            title: modelData.title
            room: ruler.xOf(next) - x - 3
        }
    }
}

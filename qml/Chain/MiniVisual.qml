import QtQuick
import FFgui.Theme

// A small picture of what a step does: an EQ curve, a compressor's transfer
// line, a fade shape, the kept part of a Trim, a level bar for gain.
Canvas {
    id: mini

    property string type
    property var params: ({})
    property real duration: 1

    onTypeChanged: requestPaint()
    onParamsChanged: requestPaint()
    onDurationChanged: requestPaint()
    onWidthChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        const p = mini.params || {}, w = width, h = height;
        const line = (fn, step) => {
            ctx.beginPath();
            for (let x = 0; x <= w; x += step || 2) {
                const y = fn(x);
                if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        };
        ctx.strokeStyle = Theme.accentGlyph;
        ctx.fillStyle = Theme.accentGlyph;
        ctx.lineWidth = 1.5;

        switch (mini.type) {
        case "trim": {
            ctx.fillStyle = Theme.border;
            ctx.fillRect(0, h / 2 - 3, w, 6);
            ctx.fillStyle = Theme.accentGlyph;
            const d = Math.max(1e-6, mini.duration);
            const a = Math.min(1, (p.start || 0) / d) * w;
            const b = (p.end > 0 ? Math.min(1, p.end / d) : 1) * w;
            ctx.fillRect(a, h / 2 - 3, Math.max(2, b - a), 6);
            break;
        }
        case "fade":
            ctx.globalAlpha = 0.3;
            ctx.beginPath();
            ctx.moveTo(0, p.t === "out" ? 2 : h - 2);
            ctx.lineTo(w, p.t === "out" ? h - 2 : 2);
            ctx.lineTo(w, h);
            ctx.lineTo(0, h);
            ctx.fill();
            break;
        case "denoise":
            line(x => h / 2 + Math.sin(x / w * Math.PI * 4) * 4 + (x < w / 2 ? Math.sin(x * 1.7) * Math.cos(x * 0.9) * 6 : 0));
            break;
        case "eq": {
            const fx = Math.log((p.f || 1000) / 20) / Math.log(1000) * w;
            const hx = Math.log((p.hp || 20) / 20) / Math.log(1000) * w;
            const q = p.w || 1;
            line(x => Math.min(h - 1, h / 2 - (p.g || 0) * 0.9 * Math.exp(-Math.pow((x - fx) / (w / 10 / q + 4), 2))
                                       + (x < hx ? (hx - x) * 0.8 : 0)));
            break;
        }
        case "highpass": {
            const hx = Math.log((p.f || 20) / 20) / Math.log(1000) * w;
            line(x => Math.min(h - 1, h / 2 + (x < hx ? (hx - x) * 0.8 : 0)));
            break;
        }
        case "lowpass": {
            const lx = Math.log((p.f || 20000) / 20) / Math.log(1000) * w;
            line(x => Math.min(h - 1, h / 2 + (x > lx ? (x - lx) * 0.8 : 0)));
            break;
        }
        case "compressor": {
            const t = ((p.threshold ?? -18) + 60) / 60, ratio = p.ratio || 2;
            ctx.beginPath();
            ctx.moveTo(0, h);
            ctx.lineTo(t * w, h - t * h);
            ctx.lineTo(w, h - t * h - (1 - t) * h / ratio);
            ctx.stroke();
            ctx.fillStyle = Theme.clip;
            ctx.fillRect(w - 6, 2, 4, Math.min(h - 4, (ratio - 1) * 2));
            break;
        }
        case "loudnorm":
        case "limiter":
        case "speechnorm":
            ctx.globalAlpha = 0.35;
            for (let x = 0; x < w; x += 5) {
                const v = Math.min(0.62, 0.25 + 0.6 * Math.abs(Math.sin(x * 0.37) * Math.cos(x * 0.11)));
                ctx.fillRect(x, h - v * h, 3, v * h);
            }
            ctx.globalAlpha = 1;
            ctx.strokeStyle = Theme.target;
            ctx.beginPath();
            ctx.moveTo(0, h * 0.38);
            ctx.lineTo(w, h * 0.38);
            ctx.stroke();
            break;
        case "gain":
            ctx.globalAlpha = 0.3;
            ctx.fillRect(0, h / 2 - (p.volume || 0) * 0.6, w, h);
            break;
        default:
            line(x => h / 2 + Math.sin(x / w * Math.PI * 6) * (h / 3));
        }
    }
}

#include "Comparer.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <tools/FfTools.h>
namespace ffgui
{
Comparer::Comparer(LogStore *log, QObject *parent) : QObject(parent), m_log(log) {}
Comparer::~Comparer() { cancel(); }
void Comparer::cancel()
{
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->kill();
        delete m_job;
        m_job = nullptr;
    }
}
void Comparer::start(const CompareSpec &s)
{
    cancel();
    m_spec = s;
    m_result.clear();
    m_a.clear();
    m_b.clear();
    m_pending.clear();
    decode();
}
QVariantMap Comparer::parseScores(const QString &text)
{
    QVariantMap out;
    for (const auto &entry : {std::pair{"SDR", "sdr"}, {"PSNR", "psnr"}}) {
        QRegularExpression re(
            QStringLiteral("%1 ch\\d+: ([+\\-0-9.einf]+) dB").arg(QLatin1String(entry.first)));
        auto all = re.globalMatch(text);
        double score = INFINITY;
        bool found = false;
        while (all.hasNext()) {
            const QString v = all.next().captured(1);
            score = std::min(score, v == u"inf" ? INFINITY : v == u"-inf" ? -INFINITY : v.toDouble());
            found = true;
        }
        if (found)
            out.insert(QLatin1String(entry.second), score);
    }
    return out;
}
double Comparer::findOffset(const std::vector<float> &a, const std::vector<float> &b, int rate,
                            double *confidence)
{
    const int hop = std::max(1, rate / 100);
    const int n = int(std::min(a.size(), b.size())) / hop;
    if (n < 10) {
        if (confidence)
            *confidence = 0;
        return 0;
    }
    std::vector<double> ea(n), eb(n);
    for (int i = 0; i < n; ++i) {
        double aa = 0, bb = 0;
        for (int j = 0; j < hop; ++j) {
            aa += double(a[size_t(i * hop + j)]) * a[size_t(i * hop + j)];
            bb += double(b[size_t(i * hop + j)]) * b[size_t(i * hop + j)];
        }
        ea[i] = std::sqrt(aa / hop);
        eb[i] = std::sqrt(bb / hop);
    }
    double best = -2;
    int shift = 0;
    for (int lag = -std::min(200, n / 4); lag <= std::min(200, n / 4); ++lag) {
        double sa = 0, sb = 0, aa = 0, bb = 0, ab = 0;
        int count = 0;
        for (int i = std::max(0, -lag); i < std::min(n, n - lag); ++i) {
            double x = ea[i], y = eb[i + lag];
            sa += x;
            sb += y;
            aa += x * x;
            bb += y * y;
            ab += x * y;
            ++count;
        }
        const double den = std::sqrt(std::max(0., aa - sa * sa / count) * std::max(0., bb - sb * sb / count));
        const double score = den > 1e-15 ? (ab - sa * sb / count) / den : 0;
        if (score > best + 1e-8 || (std::abs(score - best) < 1e-8 && std::abs(lag) < std::abs(shift))) {
            best = score;
            shift = lag;
        }
    }
    if (confidence)
        *confidence = std::max(0., best);
    return double(shift * hop) / rate;
}
void Comparer::read()
{
    if (!m_job)
        return;
    m_pending += m_job->readStdout();
    const int frames = m_pending.size() / 8;
    const auto *data = reinterpret_cast<const float *>(m_pending.constData());
    const size_t room = 480000 > m_a.size() ? 480000 - m_a.size() : 0;
    const int keep = std::min(frames, int(room));
    for (int i = 0; i < keep; ++i) {
        m_a.push_back(data[i * 2]);
        m_b.push_back(data[i * 2 + 1]);
    }
    m_pending.remove(0, frames * 8);
}
namespace
{
// "[2:a:0]" → "[3:a:0]": a graph's source references after one more input.
QString shiftInputs(const QString &filter)
{
    static const QRegularExpression input(QStringLiteral("\\[(\\d+):a:(\\d+)\\]"));
    QString shifted;
    qsizetype at = 0;
    for (auto matches = input.globalMatch(filter); matches.hasNext();) {
        const auto m = matches.next();
        shifted += filter.mid(at, m.capturedStart() - at) +
                   QStringLiteral("[%1:a:%2]").arg(m.captured(1).toInt() + 1).arg(m.captured(2));
        at = m.capturedEnd();
    }
    return shifted + filter.mid(at);
}
} // namespace
void Comparer::decode()
{
    auto chain = [this](bool b) {
        QString f = b ? m_spec.filterB : m_spec.filterA;
        const double trim = b ? std::max(0., m_spec.offset) : std::max(0., -m_spec.offset);
        if (trim > 0)
            f += (!f.isEmpty() ? "," : "") +
                 QStringLiteral("atrim=start=%1,asetpts=PTS-STARTPTS").arg(trim, 0, 'f', 6);
        if (!f.isEmpty())
            f += u',';
        return f + "aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo";
    };
    QString graph =
        QStringLiteral(
            "[0:a:%1]%2,asplit=3[at][ad][ap];[1:a:%3]%4,asplit=3[bt][bd][bp];[ad][bd]asdr,anullsink;[ap][bp]"
            "apsnr,anullsink;[at]aresample=16000,aformat=sample_fmts=fltp:channel_layouts=mono:sample_rates="
            "16000[am];[bt]aresample=16000,aformat=sample_fmts=fltp:channel_layouts=mono:sample_rates=16000["
            "bm];[am][bm]join=inputs=2:channel_layout=stereo:map=0.0-FL|1.0-FR[trace]")
            .arg(m_spec.trackA)
            .arg(chain(false))
            .arg(m_spec.trackB)
            .arg(chain(true));
    QStringList args{"-hide_banner", "-nostdin", "-loglevel", "level+info", "-i", m_spec.a};
    if (m_spec.graphB.isEmpty())
        args << "-i" << m_spec.b;
    else {
        // B is a graph plan: its sources follow A, so its input numbers shift
        // by one, and its output pad replaces B's track.
        args << m_spec.graphB.value("inputs").toStringList();
        const QString pad = m_spec.graphB.value("outputs").toMap().first().toString();
        graph.replace(QStringLiteral("[1:a:%1]").arg(m_spec.trackB), u'[' + pad + u']');
        graph = shiftInputs(m_spec.graphB.value("filter").toString()) + u';' + graph;
    }
    args << "-filter_complex" << graph << "-map" << "[trace]" << "-c:a" << "pcm_f32le" << "-f" << "f32le"
         << "pipe:1";
    auto *job = new ProcessJob(FfTools::paths().ffmpeg, args, this);
    m_job = job;
    job->setBackground(true);
    job->setLog(m_log, "compare", "warning");
    job->setKeepStderr(false);
    auto log = std::make_shared<QString>();
    connect(job, &ProcessJob::stderrLine, this, [log](const LogLine &l) {
        if ((l.text.contains("SDR ch") || l.text.contains("PSNR ch") || l.level == u"error") &&
            log->size() < 32768)
            *log += l.text + u'\n';
    });
    connect(job, &ProcessJob::stdoutReady, this, &Comparer::read);
    connect(job, &ProcessJob::finished, this, [this, job, log](bool ok) {
        read();
        m_job = nullptr;
        job->deleteLater();
        if (!ok) {
            emit finished({{"error", *log}});
            return;
        }
        m_result = parseScores(*log);
        QVariantList a, b, d;
        const int n = int(m_a.size()), hop = std::max(1, n / 600);
        double peakA = 0, peakB = 0;
        for (int i = 0; i < n; i += hop) {
            double pa = 0, pb = 0, pd = 0;
            for (int j = i; j < std::min(n, i + hop); ++j) {
                pa = std::max(pa, double(std::abs(m_a[j])));
                pb = std::max(pb, double(std::abs(m_b[j])));
                pd = std::max(pd, double(std::abs(m_a[j] - m_b[j])));
            }
            a << pa;
            b << pb;
            d << pd;
            peakA = std::max(peakA, pa);
            peakB = std::max(peakB, pb);
        }
        double confidence = 0;
        const double offset = findOffset(m_a, m_b, 16000, &confidence);
        m_result.insert("traceA", a);
        m_result.insert("traceB", b);
        m_result.insert("difference", d);
        m_result.insert("traceDuration", double(n) / 16000);
        m_result.insert("suggestedOffset", m_spec.offset + offset);
        m_result.insert("confidence", confidence);
        m_result.insert("peakChange", peakA > 0 && peakB > 0 ? 20 * std::log10(peakB / peakA) : 0);
        hash(0);
    });
    job->start();
}
void Comparer::hash(int side)
{
    const bool b = side == 1;
    QString filter = b ? m_spec.filterB : m_spec.filterA;
    const double offset = b ? std::max(0., m_spec.offset) : std::max(0., -m_spec.offset);
    if (offset > 0)
        filter += (!filter.isEmpty() ? "," : "") +
                  QStringLiteral("atrim=start=%1,asetpts=PTS-STARTPTS").arg(offset, 0, 'f', 6);
    if (!filter.isEmpty())
        filter += u',';
    filter += "aresample=48000,aformat=sample_fmts=flt:channel_layouts=stereo";
    QStringList args{"-hide_banner", "-nostdin", "-loglevel", "level+error"};
    if (b && !m_spec.graphB.isEmpty()) {
        const QString pad = m_spec.graphB.value("outputs").toMap().first().toString();
        args << m_spec.graphB.value("inputs").toStringList() << "-filter_complex"
             << m_spec.graphB.value("filter").toString() + ";[" + pad + u']' + filter + "[hash_audio]" << "-map"
             << "[hash_audio]";
    } else
        args << "-i" << (b ? m_spec.b : m_spec.a) << "-map"
             << QStringLiteral("0:a:%1").arg(b ? m_spec.trackB : m_spec.trackA) << "-af" << filter;
    args << "-c:a" << "pcm_f32le" << "-f" << "md5" << "pipe:1";
    auto *job = new ProcessJob(FfTools::paths().ffmpeg, args, this);
    m_job = job;
    job->setBackground(true);
    job->setLog(m_log, "compare hash", "error");
    auto bytes = std::make_shared<QByteArray>();
    connect(job, &ProcessJob::stdoutReady, this, [job, bytes] { *bytes += job->readStdout(); });
    connect(job, &ProcessJob::finished, this, [this, job, bytes, b](bool ok) {
        *bytes += job->readStdout();
        m_job = nullptr;
        job->deleteLater();
        if (!ok) {
            emit finished({{"error", QStringLiteral("Checksum decode failed")}});
            return;
        }
        if (!b) {
            m_hashA = QString::fromUtf8(*bytes).trimmed();
            hash(1);
        } else {
            m_result.insert("checksumA", m_hashA);
            m_result.insert("checksumB", QString::fromUtf8(*bytes).trimmed());
            m_result.insert("equal", m_hashA == QString::fromUtf8(*bytes).trimmed());
            emit finished(m_result);
        }
    });
    job->start();
}
} // namespace ffgui

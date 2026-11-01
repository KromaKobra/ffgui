#include "ProgressParser.h"

namespace ffgui {

bool ProgressParser::feed(const QByteArray &data, Snapshot *latest)
{
    m_pending += data;
    bool completed = false;
    qsizetype nl;
    while ((nl = m_pending.indexOf('\n')) >= 0) {
        const QByteArray line = m_pending.left(nl).trimmed();
        m_pending.remove(0, nl + 1);
        const qsizetype eq = line.indexOf('=');
        if (eq < 0)
            continue;
        const QByteArray key = line.left(eq);
        const QByteArray value = line.mid(eq + 1);
        if (key == "out_time_us" || key == "out_time_ms") {
            // Both are microseconds (out_time_ms is misnamed in ffmpeg).
            bool ok = false;
            const qint64 us = value.toLongLong(&ok);
            if (ok && us >= 0)
                m_current.outTime = us / 1e6;
        } else if (key == "speed") {
            QByteArray v = value;
            if (v.endsWith('x'))
                v.chop(1);
            bool ok = false;
            const double s = v.trimmed().toDouble(&ok);
            m_current.speed = ok ? s : 0;
        } else if (key == "total_size") {
            bool ok = false;
            const qint64 size = value.toLongLong(&ok);
            m_current.totalSize = ok ? size : -1;
        } else if (key == "progress") {
            m_current.end = value == "end";
            if (latest)
                *latest = m_current;
            completed = true;
        }
    }
    return completed;
}

} // namespace ffgui

#pragma once

#include <QByteArray>

// Reads ffmpeg's `-progress pipe:1` output: blocks of key=value lines, each
// ending with progress=continue or progress=end.
namespace ffgui {

class ProgressParser
{
public:
    struct Snapshot
    {
        double outTime = 0;     // seconds of output written
        double speed = 0;       // ×realtime, 0 when unknown
        qint64 totalSize = -1;  // bytes, -1 when unknown
        bool end = false;
    };

    // Feeds more output; returns true when at least one block completed, with
    // the latest in `latest`.
    bool feed(const QByteArray &data, Snapshot *latest);

private:
    QByteArray m_pending;
    Snapshot m_current;
};

} // namespace ffgui

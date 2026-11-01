#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>

// Extracts an input's attached picture (its cover art) as an image.
namespace ffgui {

class LogStore;

class CoverArt : public QObject
{
    Q_OBJECT

public:
    CoverArt(const QString &input, int streamIndex, LogStore *log = nullptr, QObject *parent = nullptr);
    void start();

signals:
    void finished(bool ok, const QImage &image);

private:
    QString m_input;
    int m_stream;
    QPointer<LogStore> m_log;
};

} // namespace ffgui

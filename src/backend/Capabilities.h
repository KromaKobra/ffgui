#pragma once

#include "Models.h"

#include <QQmlEngine>

// What the installed FFmpeg can do: where the tools are, its version, and
// which optional features this build has. Shown by the status-bar badge and
// the Capabilities panel; used to grey out steps and encoders.
class Capabilities : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    // unknown | detecting | ready | missing
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString ffmpegPath READ ffmpegPath NOTIFY changed)
    Q_PROPERTY(QString ffprobePath READ ffprobePath NOTIFY changed)
    Q_PROPERTY(QString ffplayPath READ ffplayPath NOTIFY changed)
    Q_PROPERTY(int missingCount READ missingCount NOTIFY changed)
    Q_PROPERTY(int filterCount READ filterCount NOTIFY changed)
    Q_PROPERTY(int encoderCount READ encoderCount NOTIFY changed)
    Q_PROPERTY(int muxerCount READ muxerCount NOTIFY changed)
    Q_PROPERTY(QString configuration READ configuration NOTIFY changed)
    // "FFmpeg 9.0.2 · 1 feature unavailable", or what went wrong.
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(LibrariesModel *libraries READ libraries CONSTANT)

public:
    explicit Capabilities(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString version() const { return m_version; }
    QString ffmpegPath() const { return m_ffmpegPath; }
    QString ffprobePath() const { return m_ffprobePath; }
    QString ffplayPath() const { return m_ffplayPath; }
    int missingCount() const { return m_missingCount; }
    int filterCount() const { return m_filterCount; }
    int encoderCount() const { return m_encoderCount; }
    int muxerCount() const { return m_muxerCount; }
    QString configuration() const { return m_configuration; }
    QString summary() const;
    LibrariesModel *libraries() const { return m_libraries; }

    Q_INVOKABLE bool hasFilter(const QString &name) const { return m_filters.contains(name); }
    Q_INVOKABLE bool hasEncoder(const QString &name) const { return m_encoders.contains(name); }
    Q_INVOKABLE bool hasMuxer(const QString &name) const { return m_muxers.contains(name); }
    Q_INVOKABLE void refresh();

    struct Snapshot
    {
        QString state, version, ffmpegPath, ffprobePath, ffplayPath, configuration;
        QStringList filters, encoders, muxers;
        QList<QVariantMap> libraries;
    };
    void apply(const Snapshot &snapshot);

signals:
    void changed();
    void refreshRequested();

private:
    QString m_state = QStringLiteral("unknown");
    QString m_version;
    QString m_ffmpegPath, m_ffprobePath, m_ffplayPath;
    QString m_configuration;
    QSet<QString> m_filters, m_encoders, m_muxers;
    int m_missingCount = 0;
    int m_filterCount = 0;
    int m_encoderCount = 0;
    int m_muxerCount = 0;
    LibrariesModel *m_libraries;
};

#pragma once

#include <QQmlEngine>

// The exact ffmpeg command the project builds (design principle 8), as the
// argument list that runs and as shell-quoted text to read and copy.
class Command : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(QStringList args READ args NOTIFY changed)
    // One line, e.g. "ffmpeg -hide_banner -i in.wav -c:a flac out.flac".
    Q_PROPERTY(QString text READ text NOTIFY changed)
    // One option group per line, joined with " \".
    Q_PROPERTY(QString multiline READ multiline NOTIFY changed)
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    // Why there is no command yet ("Add an input", "Choose a format").
    Q_PROPERTY(QString placeholder READ placeholder NOTIFY changed)

public:
    explicit Command(QObject *parent = nullptr);

    QStringList args() const { return m_args; }
    QString text() const { return m_text; }
    QString multiline() const { return m_multiline; }
    bool valid() const { return !m_args.isEmpty(); }
    QString placeholder() const { return m_placeholder; }

    // `groups` are option groups ("-i in.wav", "-c:a flac out.flac") already
    // quoted for display; `args` is the list that is executed.
    void set(const QStringList &args, const QStringList &groups);
    void clear(const QString &placeholder);

signals:
    void changed();

private:
    QStringList m_args;
    QString m_text;
    QString m_multiline;
    QString m_placeholder = QStringLiteral("Add an input to build a command");
};

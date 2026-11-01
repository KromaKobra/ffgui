#pragma once

#include <QQmlEngine>

// Rendering the project's outputs: progress, speed and time left while a
// render runs, and a clean stop that leaves the files valid.
class Render : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(double speed READ speed NOTIFY progressChanged)
    Q_PROPERTY(double eta READ eta NOTIFY progressChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY activeChanged)
    Q_PROPERTY(QString label READ label NOTIFY activeChanged)
    // Why Render is disabled right now, or "" when it can run.
    Q_PROPERTY(QString blocker READ blocker NOTIFY blockerChanged)

public:
    explicit Render(QObject *parent = nullptr);

    bool active() const { return m_active; }
    double progress() const { return m_progress; }
    double speed() const { return m_speed; }
    double eta() const { return m_eta; }
    int fileCount() const { return m_fileCount; }
    QString label() const { return m_label; }
    QString blocker() const { return m_blocker; }

    // scope: "all" (every output), "selection" (the selected time range only)
    Q_INVOKABLE void start(const QString &scope = QStringLiteral("all"));
    Q_INVOKABLE void stop();
    // Adds one job per input x output to the queue instead of rendering now.
    Q_INVOKABLE void enqueue();

    void setActive(bool active, int fileCount = 0, const QString &label = {});
    void setProgress(double progress, double speed, double eta);
    void setBlocker(const QString &blocker);

signals:
    void activeChanged();
    void progressChanged();
    void blockerChanged();
    void startRequested(const QString &scope);
    void stopRequested();
    void enqueueRequested();

private:
    bool m_active = false;
    double m_progress = 0;
    double m_speed = 0;
    double m_eta = 0;
    int m_fileCount = 0;
    QString m_label;
    QString m_blocker = QStringLiteral("Add an input first");
};

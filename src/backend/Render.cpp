#include "Render.h"

Render::Render(QObject *parent)
    : QObject(parent)
{
}

void Render::start(const QString &scope)
{
    if (!m_active && m_blocker.isEmpty())
        emit startRequested(scope);
}

void Render::stop()
{
    if (m_active)
        emit stopRequested();
}

void Render::enqueue()
{
    if (m_blocker.isEmpty())
        emit enqueueRequested();
}

void Render::setActive(bool active, int fileCount, const QString &label)
{
    if (m_active == active && m_fileCount == fileCount && m_label == label)
        return;
    m_active = active;
    m_fileCount = fileCount;
    m_label = label;
    if (!active)
        setProgress(0, 0, 0);
    emit activeChanged();
}

void Render::setProgress(double progress, double speed, double eta)
{
    if (m_progress == progress && m_speed == speed && m_eta == eta)
        return;
    m_progress = progress;
    m_speed = speed;
    m_eta = eta;
    emit progressChanged();
}

void Render::setBlocker(const QString &blocker)
{
    if (m_blocker == blocker)
        return;
    m_blocker = blocker;
    emit blockerChanged();
}

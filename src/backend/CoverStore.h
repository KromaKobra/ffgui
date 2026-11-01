#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

#include <memory>

// Cover art by input uid, served to QML as image://cover/<uid>.
class CoverStore
{
public:
    void set(const QString &uid, const QImage &image)
    {
        QMutexLocker lock(&m_mutex);
        m_images.insert(uid, image);
    }
    void remove(const QString &uid)
    {
        QMutexLocker lock(&m_mutex);
        m_images.remove(uid);
    }
    QImage get(const QString &uid) const
    {
        QMutexLocker lock(&m_mutex);
        return m_images.value(uid);
    }

private:
    mutable QMutex m_mutex;
    QHash<QString, QImage> m_images;
};

class CoverProvider : public QQuickImageProvider
{
public:
    explicit CoverProvider(std::shared_ptr<CoverStore> store)
        : QQuickImageProvider(QQuickImageProvider::Image)
        , m_store(std::move(store))
    {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        // "<uid>?v=2": the query only defeats QML's image cache.
        QImage image = m_store->get(id.section(u'?', 0, 0));
        if (size)
            *size = image.size();
        if (!image.isNull() && requestedSize.isValid() && !requestedSize.isEmpty())
            image = image.scaled(requestedSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        return image;
    }

private:
    std::shared_ptr<CoverStore> m_store;
};

#include "Models.h"

// Roles. Nested values (tracks, chapters, params) are lists and maps.
//
// Input tracks:   { index, codec, codecLong, profile, rate, depth, sampleFmt,
//                   layout, channels, bitrate, lang, isDefault, frames, title,
//                   disposition, sideData }
// Input chapters: { start, end, title }

InputsModel::InputsModel(QObject *parent)
    : RowModel({ "uid", "name", "path", "kind", "container", "formatName", "formatLong", "duration",
                 "size", "bitrate", "cover", "health", "healthText", "tracks", "chapters", "tags",
                 "count", "probing" }, parent)
{
}

StepsModel::StepsModel(QObject *parent)
    : RowModel({ "uid", "type", "name", "glyph", "category", "filter", "bypass", "params", "hasRange",
                 "rangeStart", "rangeEnd", "rangeSupported", "timeline", "available", "warning",
                 "formatIn", "formatOut", "formatChanged", "filterText" }, parent)
{
}

GraphModel::GraphModel(QObject *parent)
    : RowModel({ "uid", "kind", "label", "detail", "x", "y", "inputs", "outputs", "bypass" }, parent)
{
}

OutputsModel::OutputsModel(QObject *parent)
    : RowModel({ "uid", "pattern", "fileName", "folder", "preset", "container", "codec", "codecLabel",
                 "quality", "rate", "depth", "channels", "metadata", "kbps", "sizeBytes", "sizeText",
                 "reencodeReason", "lossyToLossy", "problem" }, parent)
{
}

QueueModel::QueueModel(QObject *parent)
    : RowModel({ "uid", "file", "output", "status", "progress", "speed", "eta", "error", "command" }, parent)
{
}

void QueueModel::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    emit pausedChanged();
}

LogModel::LogModel(QObject *parent)
    : RowModel({ "time", "jobId", "level", "text", "source" }, parent)
{
}

StepCatalogModel::StepCatalogModel(QObject *parent)
    : RowModel({ "key", "name", "category", "glyph", "blurb", "filter", "keywords", "timeline",
                 "available", "reason" }, parent)
{
}

PresetsModel::PresetsModel(QObject *parent)
    : RowModel({ "name", "kind", "description", "builtIn", "container", "codec", "codecLabel",
                 "quality", "rate", "depth", "channels", "steps" }, parent)
{
}

LibrariesModel::LibrariesModel(QObject *parent)
    : RowModel({ "name", "found", "unlocks", "kind" }, parent)
{
}

TranscriptModel::TranscriptModel(QObject *parent)
    : RowModel({ "start", "end", "text" }, parent)
{
}

DevicesModel::DevicesModel(QObject *parent)
    : RowModel({ "id", "name", "backend", "isDefault", "isMonitor" }, parent)
{
}

DestinationsModel::DestinationsModel(QObject *parent)
    : RowModel({ "uid", "name", "url", "protocol", "format", "status", "bitrate" }, parent)
{
}

StepFilter::StepFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &StepFilter::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &StepFilter::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &StepFilter::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &StepFilter::countChanged);
}

void StepFilter::setCategory(const QString &category)
{
    if (m_category == category)
        return;
    beginFilterChange();
    m_category = category;
    endFilterChange(Direction::Rows);
    emit categoryChanged();
}

void StepFilter::setSearch(const QString &search)
{
    if (m_search == search)
        return;
    beginFilterChange();
    m_search = search;
    endFilterChange(Direction::Rows);
    emit searchChanged();
}

QVariantMap StepFilter::get(int row) const
{
    QVariantMap values;
    const QModelIndex i = index(row, 0);
    if (!i.isValid())
        return values;
    const auto names = roleNames();
    for (auto it = names.begin(); it != names.end(); ++it)
        values.insert(QString::fromUtf8(it.value()), data(i, it.key()));
    return values;
}

bool StepFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *model = sourceModel();
    if (!model)
        return false;
    const QModelIndex i = model->index(sourceRow, 0, sourceParent);
    const auto names = model->roleNames();
    auto role = [&](const char *name) { return model->data(i, names.key(name, -1)); };

    if (!m_category.isEmpty() && role("category").toString() != m_category)
        return false;
    const QString needle = m_search.trimmed();
    if (needle.isEmpty())
        return true;
    QStringList haystack { role("name").toString(), role("filter").toString(), role("blurb").toString(),
                           role("key").toString(), role("category").toString() };
    haystack << role("keywords").toStringList();
    for (const QString &word : needle.split(u' ', Qt::SkipEmptyParts)) {
        bool found = false;
        for (const QString &h : std::as_const(haystack))
            found = found || h.contains(word, Qt::CaseInsensitive);
        if (!found)
            return false;
    }
    return true;
}

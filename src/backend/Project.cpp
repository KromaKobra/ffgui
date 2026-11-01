#include "Project.h"

Project::Project(QObject *parent)
    : QObject(parent)
    , m_inputs(new InputsModel(this))
    , m_steps(new StepsModel(this))
    , m_graph(new GraphModel(this))
    , m_outputs(new OutputsModel(this))
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, &Project::modifiedChanged);
    connect(m_inputs, &RowModel::revisionChanged, this, &Project::inputChanged);
    connect(m_steps, &RowModel::revisionChanged, this, &Project::chainChanged);
    connect(m_outputs, &RowModel::revisionChanged, this, &Project::outputsChanged);
}

void Project::setName(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    emit nameChanged();
}

void Project::setCurrentInput(int index)
{
    index = m_inputs->count() ? std::clamp(index, -1, m_inputs->count() - 1) : -1;
    if (m_currentInput == index)
        return;
    m_currentInput = index;
    emit currentInputChanged();
    emit inputChanged();
}

void Project::setCurrentOutput(int index)
{
    index = m_outputs->count() ? std::clamp(index, -1, m_outputs->count() - 1) : -1;
    if (m_currentOutput == index)
        return;
    m_currentOutput = index;
    emit currentOutputChanged();
}

QVariantMap Project::input() const
{
    return m_inputs->get(m_currentInput);
}

QVariantMap Project::track() const
{
    const QVariantList tracks = input().value(QStringLiteral("tracks")).toList();
    for (const QVariant &t : tracks)
        if (t.toMap().value(QStringLiteral("isDefault")).toBool())
            return t.toMap();
    return tracks.isEmpty() ? QVariantMap() : tracks.first().toMap();
}

double Project::duration() const
{
    return input().value(QStringLiteral("duration")).toDouble();
}

int Project::activeSteps() const
{
    int n = 0;
    for (const QVariantMap &s : m_steps->rows())
        n += s.value(QStringLiteral("bypass")).toBool() ? 0 : 1;
    return n;
}

void Project::addFiles(const QList<QUrl> &) { emit notice(tr("Adding inputs isn't available yet")); }
void Project::addFolder(const QUrl &) { emit notice(tr("Adding inputs isn't available yet")); }
void Project::addUrl(const QString &) { emit notice(tr("Adding inputs isn't available yet")); }
void Project::addGenerator(const QString &, const QString &) { emit notice(tr("Generated inputs aren't available yet")); }
void Project::removeInput(int) { }
void Project::replaceInput(int, const QUrl &) { emit notice(tr("Replacing inputs isn't available yet")); }
void Project::setCoverArt(const QUrl &) { emit notice(tr("Cover art isn't available yet")); }
void Project::useImpulseResponse(const QUrl &) { emit notice(tr("Impulse responses aren't available yet")); }
void Project::reprobe(int) { }

void Project::insertStep(int, const QString &) { emit notice(tr("Editing the chain isn't available yet")); }
void Project::addStep(const QString &, const QVariantMap &) { emit notice(tr("Editing the chain isn't available yet")); }
void Project::removeStep(int) { }
void Project::moveStep(int, int) { }
void Project::setBypass(int, bool) { }
void Project::setParam(int, const QString &, const QVariant &) { }
void Project::setRange(int, double, double) { }
void Project::clearRange(int) { }
void Project::applySelectionAction(const QString &, double, double) { emit notice(tr("Editing the chain isn't available yet")); }
void Project::applyChainPreset(const QString &) { emit notice(tr("Chain presets aren't available yet")); }

void Project::addOutput(const QString &) { emit notice(tr("Adding outputs isn't available yet")); }
void Project::removeOutput(int) { }
void Project::setOutput(int, const QVariantMap &) { }
void Project::applyPreset(int, const QString &) { }

void Project::setTag(const QString &, const QString &) { emit notice(tr("Editing tags isn't available yet")); }
void Project::removeTag(const QString &) { }

void Project::newProject() { }
bool Project::open(const QUrl &) { emit notice(tr("Opening projects isn't available yet")); return false; }
bool Project::save(const QUrl &) { emit notice(tr("Saving projects isn't available yet")); return false; }

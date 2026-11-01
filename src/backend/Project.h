#pragma once

#include "Models.h"

#include <QQmlEngine>
#include <QUndoStack>
#include <QUrl>

// The open project: inputs, the processing chain and the output targets
// (design: "a project = inputs + chain + outputs, saved as a recipe file").
// Nothing touches a source file; every change is undoable.
class Project : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)

    Q_PROPERTY(InputsModel *inputs READ inputs CONSTANT)
    Q_PROPERTY(StepsModel *steps READ steps CONSTANT)
    Q_PROPERTY(GraphModel *graph READ graph CONSTANT)
    Q_PROPERTY(OutputsModel *outputs READ outputs CONSTANT)

    Q_PROPERTY(int currentInput READ currentInput WRITE setCurrentInput NOTIFY currentInputChanged)
    Q_PROPERTY(int currentOutput READ currentOutput WRITE setCurrentOutput NOTIFY currentOutputChanged)
    // The current input's row and its default track (empty maps when none).
    Q_PROPERTY(QVariantMap input READ input NOTIFY inputChanged)
    Q_PROPERTY(QVariantMap track READ track NOTIFY inputChanged)
    Q_PROPERTY(bool hasInput READ hasInput NOTIFY inputChanged)
    Q_PROPERTY(double duration READ duration NOTIFY inputChanged)

    Q_PROPERTY(int activeSteps READ activeSteps NOTIFY chainChanged)
    // The part of the timeline a Trim step keeps (the whole input otherwise).
    Q_PROPERTY(double keptStart READ keptStart NOTIFY chainChanged)
    Q_PROPERTY(double keptEnd READ keptEnd NOTIFY chainChanged)
    // The converter ffmpeg inserts before the current output's encoder, if
    // any: { filter, from, to, reason }.
    Q_PROPERTY(QVariantMap converter READ converter NOTIFY chainChanged)
    Q_PROPERTY(QString totalSizeText READ totalSizeText NOTIFY outputsChanged)
    Q_PROPERTY(QVariantMap tags READ tags NOTIFY tagsChanged)

public:
    explicit Project(QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name);
    QUrl fileUrl() const { return m_fileUrl; }
    bool modified() const { return !m_undo.isClean(); }

    InputsModel *inputs() const { return m_inputs; }
    StepsModel *steps() const { return m_steps; }
    GraphModel *graph() const { return m_graph; }
    OutputsModel *outputs() const { return m_outputs; }
    QUndoStack *undoStack() { return &m_undo; }

    int currentInput() const { return m_currentInput; }
    void setCurrentInput(int index);
    int currentOutput() const { return m_currentOutput; }
    void setCurrentOutput(int index);
    QVariantMap input() const;
    QVariantMap track() const;
    bool hasInput() const { return m_currentInput >= 0 && m_currentInput < m_inputs->count(); }
    double duration() const;

    int activeSteps() const;
    double keptStart() const { return m_keptStart; }
    double keptEnd() const { return m_keptEnd; }
    QVariantMap converter() const { return m_converter; }
    QString totalSizeText() const { return m_totalSizeText; }
    QVariantMap tags() const { return m_tags; }

    // Inputs
    Q_INVOKABLE void addFiles(const QList<QUrl> &files);
    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void addUrl(const QString &url);
    // A lavfi source ("sine=frequency=1000:duration=5") as a new input.
    Q_INVOKABLE void addGenerator(const QString &source, const QString &name);
    Q_INVOKABLE void removeInput(int index);
    // Swaps an input's source and keeps everything else (chain, outputs).
    Q_INVOKABLE void replaceInput(int index, const QUrl &file);
    // Uses an image as the cover art written to outputs.
    Q_INVOKABLE void setCoverArt(const QUrl &image);
    // Adds a convolution step that uses this file as its impulse response.
    Q_INVOKABLE void useImpulseResponse(const QUrl &file);
    Q_INVOKABLE void reprobe(int index);

    // Chain
    Q_INVOKABLE void insertStep(int index, const QString &type);
    // Appends a step with some parameters set (e.g. a Channel mix from Routing).
    Q_INVOKABLE void addStep(const QString &type, const QVariantMap &params = {});
    Q_INVOKABLE void removeStep(int index);
    Q_INVOKABLE void moveStep(int from, int to);
    Q_INVOKABLE void setBypass(int index, bool bypass);
    Q_INVOKABLE void setParam(int index, const QString &key, const QVariant &value);
    Q_INVOKABLE void setRange(int index, double start, double end);
    Q_INVOKABLE void clearRange(int index);
    Q_INVOKABLE void applyChainPreset(const QString &name);
    // The selection toolbar: trim, cut, fadeIn, fadeOut, silence, normalize,
    // step, export, marker, applied to [start, end].
    Q_INVOKABLE void applySelectionAction(const QString &action, double start, double end);

    // Outputs
    Q_INVOKABLE void addOutput(const QString &preset = {});
    Q_INVOKABLE void removeOutput(int index);
    Q_INVOKABLE void setOutput(int index, const QVariantMap &changes);
    Q_INVOKABLE void applyPreset(int index, const QString &preset);

    // Metadata written to outputs whose policy is "edit". Starts from the
    // current input's tags.
    Q_INVOKABLE void setTag(const QString &key, const QString &value);
    Q_INVOKABLE void removeTag(const QString &key);

    // Files
    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool open(const QUrl &file);
    Q_INVOKABLE bool save(const QUrl &file = {});

signals:
    void nameChanged();
    void fileUrlChanged();
    void modifiedChanged();
    void currentInputChanged();
    void currentOutputChanged();
    void inputChanged();
    void chainChanged();
    void outputsChanged();
    void tagsChanged();
    // For things the project can't do yet, or can't do with this build.
    void notice(const QString &text);

private:
    QString m_name = QStringLiteral("Untitled project");
    QUrl m_fileUrl;
    InputsModel *m_inputs;
    StepsModel *m_steps;
    GraphModel *m_graph;
    OutputsModel *m_outputs;
    QUndoStack m_undo;
    int m_currentInput = -1;
    int m_currentOutput = -1;
    double m_keptStart = 0;
    double m_keptEnd = 0;
    QVariantMap m_converter;
    QString m_totalSizeText;
    QVariantMap m_tags;
};

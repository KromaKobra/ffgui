#pragma once

#include "ProgressParser.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>

// One run of ffmpeg, ffprobe or ffplay: an argument list (never a shell),
// stderr split into lines with the level ffmpeg tags them with (run them with
// `-loglevel level+...`), optional `-progress pipe:1` parsing, and a stop that
// lets ffmpeg finish its files ("q" on stdin, then terminate, then kill).
//
// Works asynchronously (signals, in the owner's event loop) or blocking from
// a worker thread (waitForStdout), as long as it is used from one thread.
namespace ffgui {

class LogStore;

struct LogLine
{
    QString level;      // error, warning, info, verbose, debug
    QString context;    // "Parsed_ebur128_2", "in#0", "mp3float", or ""
    QString text;
    bool continuation = false;  // carried on from the line before (no tags of its own)

    // Parses one stderr line. Lines without tags continue `previous`.
    static LogLine parse(const QString &line, const LogLine &previous);
    static LogLine parse(const QString &line) { return parse(line, LogLine()); }
    static int rank(const QString &level);  // error 0, warning 1, info 2, verbose 3, debug 4
    // Warnings that say nothing about the file's health ("Guessed Channel
    // Layout" for every WAV without a channel mask).
    static bool benign(const QString &text);
};

class ProcessJob : public QObject
{
    Q_OBJECT

public:
    ProcessJob(const QString &program, const QStringList &args, QObject *parent = nullptr);
    ~ProcessJob() override;

    // Logs the command and every stderr line at `minLevel` or worse.
    void setLog(LogStore *log, const QString &source, const QString &minLevel = QStringLiteral("warning"));
    // Treat stdout as -progress output instead of data.
    void setProgress(bool enabled) { m_progressEnabled = enabled; }
    void setWorkingDirectory(const QString &dir) { m_process.setWorkingDirectory(dir); }
    // Keep every stderr line for parsing afterwards (summaries, warnings).
    void setKeepStderr(bool keep) { m_keepStderr = keep; }
    // Run at low CPU priority (nice 10): analysis and decoding for display
    // shouldn't compete with playback or the UI.
    void setBackground(bool background);

    void start();
    qint64 writeStdin(const QByteArray &data) { return m_process.write(data); }
    bool waitForStdin(int timeoutMs) { return m_process.waitForBytesWritten(timeoutMs); }
    // Asks ffmpeg to stop cleanly ("q"), then escalates after graceMs.
    void stop(int graceMs = 1500);
    // Ends it now.
    void kill();

    // Blocking use from a worker thread: waits up to `ms` for stdout data.
    bool waitForStdout(int ms) { return m_process.waitForReadyRead(ms); }
    bool waitForFinished(int ms) { return m_process.waitForFinished(ms); }
    QByteArray readStdout(qint64 maxBytes = -1);
    qint64 stdoutAvailable() const { return m_process.bytesAvailable(); }
    bool running() const { return m_process.state() != QProcess::NotRunning; }

    QString program() const { return m_program; }
    QStringList args() const { return m_args; }
    QString commandLine() const;
    QString jobId() const { return m_jobId; }
    const QList<LogLine> &stderrLines() const { return m_lines; }
    // Error/warning lines, for messages like "Invalid data found when processing input".
    QStringList problems(const QString &worstLevel = QStringLiteral("warning")) const;
    int exitCode() const { return m_exitCode; }
    bool stopped() const { return m_stopRequested; }
    // Normal exit with code 0 (or stopped on request).
    bool succeeded() const { return m_succeeded; }
    qint64 elapsedMs() const { return m_elapsed.elapsed(); }

signals:
    void started();
    void stdoutReady();
    void progress(const ffgui::ProgressParser::Snapshot &snapshot);
    void stderrLine(const ffgui::LogLine &line);
    void finished(bool succeeded);

private:
    void readStderr();
    void handleFinished(int exitCode, QProcess::ExitStatus status);

    QString m_program;
    QStringList m_args;
    QProcess m_process;
    QPointer<LogStore> m_log;
    QString m_source;
    QString m_jobId;
    int m_logRank = 1;
    bool m_progressEnabled = false;
    bool m_keepStderr = true;
    bool m_stopRequested = false;
    bool m_succeeded = false;
    int m_exitCode = -1;
    QByteArray m_stderrPending;
    LogLine m_previous;
    QList<LogLine> m_lines;
    ProgressParser m_progressParser;
    QElapsedTimer m_elapsed;
};

} // namespace ffgui

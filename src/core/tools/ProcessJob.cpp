#include "ProcessJob.h"
#include "ShellQuote.h"

#include <log/LogStore.h>

#include <QFileInfo>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif
#include <QRegularExpression>
#include <QTimer>

namespace ffgui {

LogLine LogLine::parse(const QString &line, const LogLine &previous)
{
    // "[ctx @ 0x55d0] [level] text", "[level] text", or a bare continuation.
    static const QRegularExpression re(QStringLiteral(
        R"(^(?:\[([^\]@]+?)\s*@\s*(?:0x)?[0-9a-fA-F]+\]\s*)?\[(panic|fatal|error|warning|info|verbose|debug|trace)\]\s?(.*)$)"));
    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch())
        return { previous.level.isEmpty() ? QStringLiteral("info") : previous.level, previous.context, line, true };
    QString level = m.captured(2);
    if (level == u"panic" || level == u"fatal")
        level = QStringLiteral("error");
    else if (level == u"trace")
        level = QStringLiteral("debug");
    return { level, m.captured(1).trimmed(), m.captured(3), false };
}

int LogLine::rank(const QString &level)
{
    if (level == u"error")
        return 0;
    if (level == u"warning")
        return 1;
    if (level == u"info")
        return 2;
    if (level == u"verbose")
        return 3;
    return 4;
}

bool LogLine::benign(const QString &text)
{
    return text.trimmed().startsWith(QStringLiteral("Guessed Channel Layout"));
}

ProcessJob::ProcessJob(const QString &program, const QStringList &args, QObject *parent)
    : QObject(parent)
    , m_program(program)
    , m_args(args)
{
    m_process.setProgram(program);
    m_process.setArguments(args);
    connect(&m_process, &QProcess::readyReadStandardError, this, &ProcessJob::readStderr);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (!m_progressEnabled) {
            emit stdoutReady();
            return;
        }
        ProgressParser::Snapshot s;
        if (m_progressParser.feed(m_process.readAllStandardOutput(), &s))
            emit progress(s);
    });
    connect(&m_process, &QProcess::finished, this, &ProcessJob::handleFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        const QString message = QStringLiteral("Couldn't start %1: %2").arg(m_program, m_process.errorString());
        m_lines << LogLine { QStringLiteral("error"), {}, message, false };
        if (m_log)
            m_log->append(m_jobId, QStringLiteral("error"), message, m_source);
        m_succeeded = false;
        emit finished(false);
    });
}

ProcessJob::~ProcessJob()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

void ProcessJob::setBackground(bool background)
{
#ifdef Q_OS_UNIX
    if (background)
        m_process.setChildProcessModifier([] { (void)::nice(10); });
    else
        m_process.setChildProcessModifier({});
#else
    Q_UNUSED(background)
#endif
}

void ProcessJob::setLog(LogStore *log, const QString &source, const QString &minLevel)
{
    m_log = log;
    m_source = source;
    m_logRank = LogLine::rank(minLevel);
}

QString ProcessJob::commandLine() const
{
    QStringList all { QFileInfo(m_program).fileName() };
    all += m_args;
    return shellJoin(all);
}

void ProcessJob::start()
{
    if (m_log)
        m_jobId = m_log->newJob(m_source, commandLine());
    m_elapsed.start();
    m_process.start(QIODevice::ReadWrite);
    emit started();
}

void ProcessJob::stop(int graceMs)
{
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_stopRequested = true;
    m_process.write("q");
    m_process.closeWriteChannel();
    QPointer<ProcessJob> self(this);
    QTimer::singleShot(graceMs, this, [self] {
        if (self && self->m_process.state() != QProcess::NotRunning)
            self->m_process.terminate();
    });
    QTimer::singleShot(graceMs * 2, this, [self] {
        if (self && self->m_process.state() != QProcess::NotRunning)
            self->m_process.kill();
    });
}

void ProcessJob::kill()
{
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_stopRequested = true;
    m_process.kill();
}

QByteArray ProcessJob::readStdout(qint64 maxBytes)
{
    return maxBytes < 0 ? m_process.readAllStandardOutput() : m_process.read(maxBytes);
}

QStringList ProcessJob::problems(const QString &worstLevel) const
{
    const int limit = LogLine::rank(worstLevel);
    QStringList out;
    for (const LogLine &l : m_lines)
        if (LogLine::rank(l.level) <= limit && !l.text.trimmed().isEmpty())
            out << l.text.trimmed();
    return out;
}

void ProcessJob::readStderr()
{
    m_stderrPending += m_process.readAllStandardError();
    qsizetype nl;
    while ((nl = m_stderrPending.indexOf('\n')) >= 0) {
        QString text = QString::fromUtf8(m_stderrPending.left(nl));
        m_stderrPending.remove(0, nl + 1);
        if (text.endsWith(u'\r'))
            text.chop(1);
        if (text.trimmed().isEmpty())
            continue;
        const LogLine line = LogLine::parse(text, m_previous);
        m_previous = line;
        if (m_keepStderr)
            m_lines << line;
        if (m_log && LogLine::rank(line.level) <= m_logRank)
            m_log->append(m_jobId, line.level,
                          line.context.isEmpty() ? line.text : u'[' + line.context + QStringLiteral("] ") + line.text,
                          m_source);
        emit stderrLine(line);
    }
}

void ProcessJob::handleFinished(int exitCode, QProcess::ExitStatus status)
{
    readStderr();
    if (!m_stderrPending.trimmed().isEmpty()) {
        m_stderrPending += '\n';
        readStderr();
    }
    if (m_progressEnabled) {
        ProgressParser::Snapshot s;
        if (m_progressParser.feed(m_process.readAllStandardOutput() + '\n', &s))
            emit progress(s);
    }
    m_exitCode = exitCode;
    m_succeeded = m_stopRequested || (status == QProcess::NormalExit && exitCode == 0);
    if (m_log && !m_stopRequested && !m_succeeded)
        m_log->append(m_jobId, QStringLiteral("error"),
                      status == QProcess::CrashExit ? QStringLiteral("%1 crashed").arg(QFileInfo(m_program).fileName())
                                                    : QStringLiteral("%1 exited with code %2").arg(QFileInfo(m_program).fileName()).arg(exitCode),
                      m_source);
    emit finished(m_succeeded);
}

} // namespace ffgui

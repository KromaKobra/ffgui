#include <QDir>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQuickTest>

#include "ffgui_qml_plugins.h"

// Matches main.cpp's setup, and hands the tests a few things through
// `testEnv`:
//   screenshotDir   FFGUI_SCREENSHOT_DIR: where tst_workspaces saves its shots
//   loadFixture     a small generated file to load (empty without ffmpeg):
//                   4 s FLAC in Matroska, a title tag, two chapters, 1.5 s of
//                   1 kHz at -20 dBFS then 2.5 s of silence
class Setup : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QGuiApplication::setOrganizationName(QStringLiteral("ffgui-tests"));
        QGuiApplication::setApplicationName(QStringLiteral("ffgui-tests"));
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
        m_fixture = makeFixture();
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(
            QStringLiteral("testEnv"),
            QVariantMap { { "screenshotDir", qEnvironmentVariable("FFGUI_SCREENSHOT_DIR") },
                          { "loadFixture", m_fixture } });
    }

private:
    QString makeFixture()
    {
        const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (ffmpeg.isEmpty() || !m_dir.isValid())
            return {};
        const QString meta = m_dir.filePath(QStringLiteral("meta.txt"));
        QFile f(meta);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(";FFMETADATA1\ntitle=Fixture\n"
                "[CHAPTER]\nTIMEBASE=1/1000\nSTART=0\nEND=1500\ntitle=Tone\n"
                "[CHAPTER]\nTIMEBASE=1/1000\nSTART=1500\nEND=4000\ntitle=Quiet\n");
        f.close();
        const QString out = m_dir.filePath(QStringLiteral("fixture.mka"));
        QProcess p;
        p.start(ffmpeg, { "-hide_banner", "-v", "error", "-y",
                          "-f", "lavfi", "-i", "sine=f=1000:r=48000:d=1.5,volume=0.8",
                          "-f", "lavfi", "-i", "anullsrc=r=48000:cl=mono:d=2.5",
                          "-i", meta,
                          "-filter_complex", "[0][1]concat=n=2:v=0:a=1,pan=stereo|c0=c0|c1=c0[a]",
                          "-map", "[a]", "-map_metadata", "2", "-map_chapters", "2",
                          "-c:a", "flac", out });
        return p.waitForFinished(20000) && p.exitCode() == 0 ? out : QString();
    }

    QTemporaryDir m_dir;
    QString m_fixture;
};

QUICK_TEST_MAIN_WITH_SETUP(ffgui, Setup)
#include "main.moc"

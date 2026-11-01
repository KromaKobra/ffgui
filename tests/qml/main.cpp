#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QtQuickTest>

#include "ffgui_qml_plugins.h"

// Matches main.cpp's setup, and hands the tests a few settings from the
// environment (FFGUI_SCREENSHOT_DIR: where tst_workspaces saves its shots).
class Setup : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QGuiApplication::setOrganizationName(QStringLiteral("ffgui-tests"));
        QGuiApplication::setApplicationName(QStringLiteral("ffgui-tests"));
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(
            QStringLiteral("testEnv"),
            QVariantMap { { "screenshotDir", qEnvironmentVariable("FFGUI_SCREENSHOT_DIR") } });
    }
};

QUICK_TEST_MAIN_WITH_SETUP(ffgui, Setup)
#include "main.moc"

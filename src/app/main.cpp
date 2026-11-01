#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "ffgui_qml_plugins.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("ffgui"));
    QGuiApplication::setApplicationName(QStringLiteral("ffgui"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("ffgui"));
    QGuiApplication::setApplicationVersion(QStringLiteral(FFGUI_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("ffgui"));

    // The FFgui.Controls facade themes Fusion controls; set it explicitly so
    // the platform style never picks something else.
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("FFgui.Shell", "Main");

    return app.exec();
}

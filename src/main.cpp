// Omonitor — arrange Hyprland displays from an Omarchy-native interface.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("omonitor");
    app.setApplicationDisplayName("Omonitor");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("omonitor");
    app.setWindowIcon(QIcon::fromTheme("omonitor"));

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);
    Theme theme(qEnvironmentVariable("OMONITOR_THEME_DIR"));
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));

    if (engine.rootObjects().isEmpty())
        return 1;

    const QString screenshot = qEnvironmentVariable("OMONITOR_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QTimer::singleShot(800, &app, [window, screenshot, &app] {
            if (window)
                window->grabWindow().save(screenshot);
            app.quit();
        });
    }

    return app.exec();
}

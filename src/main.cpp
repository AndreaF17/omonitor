// Omonitor — arrange Hyprland displays from an Omarchy-native interface.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "theme.h"
#include "tui.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    const QString command = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString("open");
    const bool terminal = command == "tui";
    if (command != "open") {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qunsetenv("QT_QPA_PLATFORMTHEME");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("omonitor");
    app.setApplicationDisplayName("Omonitor");
    app.setApplicationVersion("0.2.0");
    app.setDesktopFileName("omonitor");
    app.setWindowIcon(QIcon::fromTheme("omonitor"));

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);
    Backend backend(nullptr, {}, {}, !terminal);

    if (terminal)
        return runTui(backend);
    if (command == "--help" || command == "help") {
        QTextStream(stdout) << "Usage: omonitor [open|tui|--version]\n";
        return 0;
    }
    if (command == "--version") {
        QTextStream(stdout) << app.applicationVersion() << '\n';
        return 0;
    }
    if (command != "open") {
        QTextStream(stderr) << "Unknown command: " << command << '\n';
        return 1;
    }

    Theme theme(qEnvironmentVariable("OMONITOR_THEME_DIR"));

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

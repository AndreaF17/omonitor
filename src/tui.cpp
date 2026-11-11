#include "tui.h"

#include "backend.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {
QString text(const char *source)
{
    return QCoreApplication::translate("Tui", source);
}

void clearScreen(QTextStream &out)
{
    out << "\033[2J\033[H";
}

void pause(QTextStream &in, QTextStream &out)
{
    out << '\n' << text("Press Enter to continue") << "…" << Qt::flush;
    in.readLine();
}

int readMonitor(QTextStream &in, QTextStream &out, const Backend &backend, const QString &prompt)
{
    out << prompt << " [1-" << backend.monitors().size() << "]: " << Qt::flush;
    bool ok = false;
    const int value = in.readLine().trimmed().toInt(&ok) - 1;
    return ok && value >= 0 && value < backend.monitors().size() ? value : -1;
}

void printLayout(QTextStream &out, const Backend &backend)
{
    out << "\033[1;33mOMONITOR\033[0m  " << text("Terminal display arranger") << "\n\n";
    const QVariantList monitors = backend.monitors();
    for (int i = 0; i < monitors.size(); ++i) {
        const QVariantMap monitor = monitors.at(i).toMap();
        out << "  \033[1m" << i + 1 << ". " << monitor.value("name").toString() << "\033[0m  "
            << monitor.value("mode").toString() << "  "
            << monitor.value("scale").toDouble() << "×  @ "
            << monitor.value("x").toInt() << QStringLiteral("×") << monitor.value("y").toInt() << '\n'
            << "     " << monitor.value("description").toString() << '\n';
    }
    out << '\n';
}

void moveDisplay(QTextStream &in, QTextStream &out, Backend &backend)
{
    const int moving = readMonitor(in, out, backend, text("Display to move"));
    if (moving < 0)
        return;
    const int reference = readMonitor(in, out, backend, text("Place it next to display"));
    if (reference < 0 || reference == moving)
        return;
    out << text("Direction") << " [l=" << text("left") << ", r=" << text("right")
        << ", u=" << text("above") << ", d=" << text("below") << "]: " << Qt::flush;
    const QString choice = in.readLine().trimmed().toLower();
    if (choice == "l") backend.placeRelative(moving, reference, "left");
    else if (choice == "r") backend.placeRelative(moving, reference, "right");
    else if (choice == "u") backend.placeRelative(moving, reference, "above");
    else if (choice == "d") backend.placeRelative(moving, reference, "below");
}

void configureDisplay(QTextStream &in, QTextStream &out, Backend &backend)
{
    const int index = readMonitor(in, out, backend, text("Display to configure"));
    if (index < 0)
        return;
    const QVariantMap monitor = backend.monitors().at(index).toMap();
    const QStringList modes = monitor.value("modes").toStringList();
    out << '\n' << text("Available modes") << ":\n";
    for (int i = 0; i < modes.size(); ++i)
        out << "  " << i + 1 << ". " << modes.at(i) << '\n';
    out << text("Mode number (Enter keeps current)") << ": " << Qt::flush;
    const QString modeText = in.readLine().trimmed();
    bool modeOk = false;
    const int mode = modeText.toInt(&modeOk) - 1;
    if (modeOk && mode >= 0 && mode < modes.size())
        backend.setMode(index, modes.at(mode));

    out << text("Scale (Enter keeps current)") << " [0.75-2.00]: " << Qt::flush;
    const QString scaleText = in.readLine().trimmed();
    bool scaleOk = false;
    const double scale = scaleText.toDouble(&scaleOk);
    if (scaleOk)
        backend.setScale(index, scale);

    out << text("Orientation") << " [0=" << text("landscape") << ", 1=90°, 2=180°, 3=270°; Enter="
        << text("keep") << "]: " << Qt::flush;
    const QString transformText = in.readLine().trimmed();
    bool transformOk = false;
    const int transform = transformText.toInt(&transformOk);
    if (transformOk)
        backend.setTransform(index, transform);
    backend.compactLayout();
}

void saveLayout(QTextStream &in, QTextStream &out, Backend &backend)
{
    backend.apply();
    if (!backend.error().isEmpty()) {
        out << "\n\033[1;31m" << text("Apply failed") << ":\033[0m " << backend.error() << '\n';
        return;
    }
    out << '\n' << text("Keep this display configuration?") << " [y/N]: " << Qt::flush;
    const QString answer = in.readLine().trimmed().toLower();
    if (answer == "y" || answer == "yes" || answer == "s" || answer == "si" || answer == "sì") {
        backend.confirm();
        out << "\033[1;32m" << text("Display configuration saved") << "\033[0m\n";
    } else {
        backend.revert();
        out << text("Previous display configuration restored") << '\n';
    }
}
}

int runTui(Backend &backend)
{
    QTextStream in(stdin);
    QTextStream out(stdout);
    backend.refresh();

    while (true) {
        clearScreen(out);
        printLayout(out, backend);
        if (!backend.error().isEmpty())
            out << "\033[1;31m" << backend.error() << "\033[0m\n\n";
        out << "  [a] " << text("Arrange without gaps") << '\n'
            << "  [m] " << text("Move a display") << '\n'
            << "  [c] " << text("Configure a display") << '\n'
            << "  [r] " << text("Detect displays again") << '\n'
            << "  [s] " << text("Apply and save") << '\n'
            << "  [q] " << text("Quit") << "\n\n> " << Qt::flush;

        const QString action = in.readLine().trimmed().toLower();
        if (action == "q")
            return 0;
        if (action == "a") backend.compactLayout();
        else if (action == "m") moveDisplay(in, out, backend);
        else if (action == "c") configureDisplay(in, out, backend);
        else if (action == "r") backend.refresh();
        else if (action == "s") saveLayout(in, out, backend);
        else continue;
        pause(in, out);
    }
}

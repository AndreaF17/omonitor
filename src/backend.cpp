#include "backend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPoint>
#include <QProcess>
#include <QRect>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr auto StartMarker = "-- omonitor:start";
constexpr auto EndMarker = "-- omonitor:end";

QString luaString(QString value)
{
    value.replace('\\', "\\\\");
    value.replace('"', "\\\"");
    return '"' + value + '"';
}

QString currentMode(const QJsonObject &object)
{
    const int width = object.value("width").toInt();
    const int height = object.value("height").toInt();
    const double refresh = object.value("refreshRate").toDouble();
    return QString("%1x%2@%3").arg(width).arg(height).arg(refresh, 0, 'f', 2);
}
}

Backend::Backend(QObject *parent, const QString &configPath, const QString &hyprctl, bool autoRefresh)
    : QObject(parent),
      m_configPath(!configPath.isEmpty() ? configPath
                   : !qEnvironmentVariableIsEmpty("OMONITOR_CONFIG")
                       ? qEnvironmentVariable("OMONITOR_CONFIG")
                       : QDir::homePath() + "/.config/hypr/monitors.lua"),
      m_hyprctl(!hyprctl.isEmpty() ? hyprctl
                : !qEnvironmentVariableIsEmpty("OMONITOR_HYPRCTL")
                    ? qEnvironmentVariable("OMONITOR_HYPRCTL")
                    : QStandardPaths::findExecutable("hyprctl"))
{
    m_revertTimer.setSingleShot(true);
    m_revertTimer.setInterval(15000);
    connect(&m_revertTimer, &QTimer::timeout, this, &Backend::revert);

    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, [this] {
        if (m_secondsRemaining > 0) {
            --m_secondsRemaining;
            emit secondsRemainingChanged();
        }
    });

    // Connector names change only when a display is added or removed. Polling
    // that compact signature avoids overwriting layout edits while still making
    // hotplug detection reliable across docks and GPU drivers.
    m_detectionTimer.setInterval(1500);
    connect(&m_detectionTimer, &QTimer::timeout, this, &Backend::detectMonitorChanges);

    if (autoRefresh) {
        QTimer::singleShot(0, this, &Backend::refresh);
        m_detectionTimer.start();
    }
}

QByteArray Backend::runHyprctl(const QStringList &arguments, bool *ok) const
{
    QProcess process;
    process.start(m_hyprctl, arguments);
    const bool finished = !m_hyprctl.isEmpty() && process.waitForFinished(2500);
    const bool success = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (ok)
        *ok = success;
    return success ? process.readAllStandardOutput() : process.readAllStandardError();
}

void Backend::refresh()
{
    bool ok = false;
    const QByteArray output = runHyprctl({"-j", "monitors", "all"}, &ok);
    if (!ok) {
        setError(tr("Could not read monitors. Is Hyprland running?"));
        return;
    }
    if (!loadFromJson(output))
        setError(tr("Hyprland returned monitor data in an unexpected format."));
}

QString Backend::monitorSignature(const QByteArray &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isArray())
        return {};
    QStringList names;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        if (!object.value("disabled").toBool())
            names << object.value("name").toString();
    }
    names.sort();
    return names.isEmpty() ? QStringLiteral("<none>") : names.join('|');
}

void Backend::detectMonitorChanges()
{
    if (m_pendingConfirmation)
        return;
    bool ok = false;
    const QByteArray output = runHyprctl({"-j", "monitors", "all"}, &ok);
    if (!ok)
        return;
    const QString signature = monitorSignature(output);
    if (!signature.isEmpty() && signature != m_monitorSignature) {
        loadFromJson(output);
        setStatus(tr("Display change detected"));
    }
}

bool Backend::loadFromJson(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return false;

    QVariantList monitors;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        if (object.value("name").toString().isEmpty() || object.value("disabled").toBool())
            continue;

        QStringList modes;
        for (const QJsonValue &mode : object.value("availableModes").toArray()) {
            QString name = mode.toString();
            name.remove("Hz", Qt::CaseInsensitive);
            modes << name;
        }
        const QString selected = currentMode(object);
        if (!modes.contains(selected))
            modes.prepend(selected);

        QVariantMap monitor;
        monitor.insert("name", object.value("name").toString());
        monitor.insert("description", object.value("description").toString());
        monitor.insert("width", object.value("width").toInt());
        monitor.insert("height", object.value("height").toInt());
        monitor.insert("x", object.value("x").toInt());
        monitor.insert("y", object.value("y").toInt());
        monitor.insert("scale", object.value("scale").toDouble(1.0));
        monitor.insert("transform", object.value("transform").toInt());
        monitor.insert("focused", object.value("focused").toBool());
        monitor.insert("mode", selected);
        monitor.insert("modes", modes);
        monitors << monitor;
    }

    m_monitors = monitors;
    m_monitorSignature = monitorSignature(json);
    emit monitorsChanged();
    setError({});
    setStatus(tr("Displays refreshed"));
    return true;
}

bool Backend::validIndex(int index) const
{
    return index >= 0 && index < m_monitors.size();
}

void Backend::updateMonitor(int index, const QVariantMap &monitor)
{
    if (!validIndex(index))
        return;
    m_monitors[index] = monitor;
    emit monitorsChanged();
}

void Backend::setPosition(int index, int x, int y)
{
    if (!validIndex(index))
        return;
    QVariantMap monitor = m_monitors.at(index).toMap();
    monitor["x"] = x;
    monitor["y"] = y;
    updateMonitor(index, monitor);
}

QSize Backend::logicalSize(const QVariantMap &monitor)
{
    const bool rotated = monitor.value("transform").toInt() % 2 == 1;
    const double scale = std::max(0.5, monitor.value("scale").toDouble());
    const int width = rotated ? monitor.value("height").toInt() : monitor.value("width").toInt();
    const int height = rotated ? monitor.value("width").toInt() : monitor.value("height").toInt();
    return QSize(qRound(width / scale), qRound(height / scale));
}

void Backend::normalizePositions()
{
    if (m_monitors.isEmpty())
        return;
    int minimumX = std::numeric_limits<int>::max();
    int minimumY = std::numeric_limits<int>::max();
    for (const QVariant &value : m_monitors) {
        const QVariantMap monitor = value.toMap();
        minimumX = std::min(minimumX, monitor.value("x").toInt());
        minimumY = std::min(minimumY, monitor.value("y").toInt());
    }
    if (minimumX == 0 && minimumY == 0)
        return;
    for (int i = 0; i < m_monitors.size(); ++i) {
        QVariantMap monitor = m_monitors.at(i).toMap();
        monitor["x"] = monitor.value("x").toInt() - minimumX;
        monitor["y"] = monitor.value("y").toInt() - minimumY;
        m_monitors[i] = monitor;
    }
}

void Backend::snapPosition(int index, int x, int y)
{
    if (!validIndex(index))
        return;
    if (m_monitors.size() == 1) {
        setPosition(index, 0, 0);
        return;
    }

    const QSize movingSize = logicalSize(m_monitors.at(index).toMap());
    struct Candidate { int x; int y; long long score; bool overlaps; };
    QList<Candidate> candidates;
    auto clamped = [](int value, int low, int high) {
        return std::max(low, std::min(value, high));
    };
    auto snapCrossAxis = [&](int value, int movingLength, int otherStart, int otherLength) {
        constexpr int AlignmentDistance = 240;
        if (std::abs(value - otherStart) <= AlignmentDistance)
            return otherStart;
        const int alignedEnd = otherStart + otherLength - movingLength;
        if (std::abs(value - alignedEnd) <= AlignmentDistance)
            return alignedEnd;
        return clamped(value, otherStart - movingLength + 1, otherStart + otherLength - 1);
    };

    for (int otherIndex = 0; otherIndex < m_monitors.size(); ++otherIndex) {
        if (otherIndex == index)
            continue;
        const QVariantMap other = m_monitors.at(otherIndex).toMap();
        const QSize otherSize = logicalSize(other);
        const int ox = other.value("x").toInt();
        const int oy = other.value("y").toInt();
        const QList<QPoint> points{
            {ox - movingSize.width(), snapCrossAxis(y, movingSize.height(), oy, otherSize.height())},
            {ox + otherSize.width(), snapCrossAxis(y, movingSize.height(), oy, otherSize.height())},
            {snapCrossAxis(x, movingSize.width(), ox, otherSize.width()), oy - movingSize.height()},
            {snapCrossAxis(x, movingSize.width(), ox, otherSize.width()), oy + otherSize.height()}
        };
        for (const QPoint &point : points) {
            const long long dx = point.x() - x;
            const long long dy = point.y() - y;
            bool overlaps = false;
            const QRect placed(point, movingSize);
            for (int check = 0; check < m_monitors.size(); ++check) {
                if (check == index)
                    continue;
                const QVariantMap existing = m_monitors.at(check).toMap();
                const QRect occupied(QPoint(existing.value("x").toInt(), existing.value("y").toInt()),
                                     logicalSize(existing));
                if (placed.intersects(occupied)) {
                    overlaps = true;
                    break;
                }
            }
            candidates << Candidate{point.x(), point.y(), dx * dx + dy * dy, overlaps};
        }
    }

    const auto best = std::min_element(candidates.begin(), candidates.end(), [](const Candidate &left,
                                                                                 const Candidate &right) {
        if (left.overlaps != right.overlaps)
            return !left.overlaps;
        return left.score < right.score;
    });
    if (best == candidates.end())
        return;
    QVariantMap monitor = m_monitors.at(index).toMap();
    monitor["x"] = best->x;
    monitor["y"] = best->y;
    m_monitors[index] = monitor;
    normalizePositions();
    emit monitorsChanged();
    setStatus(tr("Display snapped to its nearest neighbor"));
}

void Backend::setMode(int index, const QString &mode)
{
    if (!validIndex(index) || mode.isEmpty())
        return;
    QVariantMap monitor = m_monitors.at(index).toMap();
    monitor["mode"] = mode;
    const QRegularExpression expression("^(\\d+)x(\\d+)@");
    const auto match = expression.match(mode);
    if (match.hasMatch()) {
        monitor["width"] = match.captured(1).toInt();
        monitor["height"] = match.captured(2).toInt();
    }
    updateMonitor(index, monitor);
}

void Backend::setScale(int index, double scale)
{
    if (!validIndex(index) || scale < 0.5 || scale > 4.0)
        return;
    QVariantMap monitor = m_monitors.at(index).toMap();
    monitor["scale"] = scale;
    updateMonitor(index, monitor);
}

void Backend::setTransform(int index, int transform)
{
    if (!validIndex(index) || transform < 0 || transform > 3)
        return;
    QVariantMap monitor = m_monitors.at(index).toMap();
    monitor["transform"] = transform;
    updateMonitor(index, monitor);
}

void Backend::alignTop(int index, int referenceIndex)
{
    if (!validIndex(index) || !validIndex(referenceIndex) || index == referenceIndex)
        return;
    QVariantMap monitor = m_monitors.at(index).toMap();
    monitor["y"] = m_monitors.at(referenceIndex).toMap().value("y").toInt();
    updateMonitor(index, monitor);
}

void Backend::placeRelative(int index, int referenceIndex, const QString &direction)
{
    if (!validIndex(index) || !validIndex(referenceIndex) || index == referenceIndex)
        return;
    QVariantMap monitor = m_monitors.at(index).toMap();
    const QVariantMap reference = m_monitors.at(referenceIndex).toMap();
    const QSize size = logicalSize(monitor);
    const QSize referenceSize = logicalSize(reference);
    const int rx = reference.value("x").toInt();
    const int ry = reference.value("y").toInt();
    if (direction == "left") {
        monitor["x"] = rx - size.width();
        monitor["y"] = ry;
    } else if (direction == "right") {
        monitor["x"] = rx + referenceSize.width();
        monitor["y"] = ry;
    } else if (direction == "above") {
        monitor["x"] = rx;
        monitor["y"] = ry - size.height();
    } else if (direction == "below") {
        monitor["x"] = rx;
        monitor["y"] = ry + referenceSize.height();
    } else {
        return;
    }
    m_monitors[index] = monitor;
    normalizePositions();
    emit monitorsChanged();
}

void Backend::compactLayout()
{
    if (m_monitors.isEmpty())
        return;

    struct Entry { int index; int x; int centerY; int width; int height; };
    QList<Entry> entries;
    for (int i = 0; i < m_monitors.size(); ++i) {
        const QVariantMap monitor = m_monitors.at(i).toMap();
        const QSize size = logicalSize(monitor);
        entries << Entry{i, monitor.value("x").toInt(), monitor.value("y").toInt() + size.height() / 2,
                         size.width(), size.height()};
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &left, const Entry &right) {
        if (left.centerY != right.centerY)
            return left.centerY < right.centerY;
        return left.x < right.x;
    });

    QList<QList<Entry>> rows;
    for (const Entry &entry : entries) {
        if (rows.isEmpty()) {
            rows << QList<Entry>{entry};
            continue;
        }
        const QList<Entry> &last = rows.last();
        int centerSum = 0;
        int tallest = 0;
        for (const Entry &member : last) {
            centerSum += member.centerY;
            tallest = std::max(tallest, member.height);
        }
        const int rowCenter = centerSum / last.size();
        if (std::abs(entry.centerY - rowCenter) <= std::max(tallest, entry.height) / 2)
            rows.last() << entry;
        else
            rows << QList<Entry>{entry};
    }

    int nextY = 0;
    for (QList<Entry> &row : rows) {
        std::sort(row.begin(), row.end(), [](const Entry &left, const Entry &right) { return left.x < right.x; });
        int nextX = 0;
        int rowHeight = 0;
        for (const Entry &entry : row) {
            QVariantMap monitor = m_monitors.at(entry.index).toMap();
            monitor["x"] = nextX;
            monitor["y"] = nextY;
            m_monitors[entry.index] = monitor;
            nextX += entry.width;
            rowHeight = std::max(rowHeight, entry.height);
        }
        nextY += rowHeight;
    }
    emit monitorsChanged();
    setStatus(tr("Displays arranged without gaps"));
}

QString Backend::managedBlock(const QVariantList &monitors)
{
    QStringList lines{StartMarker, "-- Managed by Omonitor. Manual configuration outside this block is preserved."};
    for (const QVariant &value : monitors) {
        const QVariantMap monitor = value.toMap();
        lines << QString("hl.monitor({ output = %1, mode = %2, position = %3, scale = %4, transform = %5 })")
                     .arg(luaString(monitor.value("name").toString()),
                          luaString(monitor.value("mode").toString()),
                          luaString(QString("%1x%2").arg(monitor.value("x").toInt()).arg(monitor.value("y").toInt())),
                          QString::number(monitor.value("scale").toDouble(), 'g', 3),
                          QString::number(monitor.value("transform").toInt()));
    }
    lines << EndMarker;
    return lines.join('\n') + '\n';
}

QString Backend::replaceManagedBlock(const QString &contents, const QString &block)
{
    const QRegularExpression expression(
        QString("(?:^|\\n)%1.*?%2(?:\\n|$)").arg(QRegularExpression::escape(StartMarker),
                                                    QRegularExpression::escape(EndMarker)),
        QRegularExpression::DotMatchesEverythingOption);
    QString result = contents;
    const auto match = expression.match(result);
    if (match.hasMatch()) {
        const int leadingNewline = result.mid(match.capturedStart(), 1) == "\n" ? 1 : 0;
        result.replace(match.capturedStart() + leadingNewline,
                       match.capturedLength() - leadingNewline, block);
    } else {
        if (!result.isEmpty() && !result.endsWith('\n'))
            result += '\n';
        if (!result.isEmpty())
            result += '\n';
        result += block;
    }
    return result;
}

bool Backend::writeConfig(const QString &contents)
{
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    QSaveFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    if (file.write(contents.toUtf8()) < 0)
        return false;
    return file.commit();
}

void Backend::apply()
{
    if (m_monitors.isEmpty()) {
        setError(tr("No active display to save."));
        return;
    }

    QFile file(m_configPath);
    QString current;
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setError(tr("Could not read the monitor configuration."));
            return;
        }
        current = QString::fromUtf8(file.readAll());
    }
    m_previousContents = current;

    const QString backup = m_configPath + ".omonitor." +
                           QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".bak";
    if (file.exists())
        QFile::copy(m_configPath, backup);

    if (!writeConfig(replaceManagedBlock(current, managedBlock(m_monitors)))) {
        setError(tr("Could not write the monitor configuration."));
        return;
    }

    bool reloadOk = false;
    runHyprctl({"reload"}, &reloadOk);
    bool errorsOk = false;
    const QString configErrors = QString::fromUtf8(runHyprctl({"configerrors"}, &errorsOk)).trimmed();
    if (!reloadOk || !errorsOk || !configErrors.isEmpty()) {
        writeConfig(m_previousContents);
        runHyprctl({"reload"});
        setError(configErrors.isEmpty() ? tr("Hyprland rejected the new configuration.") : configErrors);
        m_previousContents.clear();
        return;
    }

    setError({});
    setStatus(tr("Keep this display configuration?"));
    startConfirmation();
}

void Backend::startConfirmation()
{
    m_pendingConfirmation = true;
    m_secondsRemaining = 15;
    emit pendingConfirmationChanged();
    emit secondsRemainingChanged();
    m_revertTimer.start();
    m_countdownTimer.start();
}

void Backend::confirm()
{
    if (!m_pendingConfirmation)
        return;
    m_revertTimer.stop();
    m_countdownTimer.stop();
    m_pendingConfirmation = false;
    m_secondsRemaining = 0;
    m_previousContents.clear();
    emit pendingConfirmationChanged();
    emit secondsRemainingChanged();
    setStatus(tr("Display configuration saved"));
}

void Backend::revert()
{
    if (!m_pendingConfirmation)
        return;
    m_revertTimer.stop();
    m_countdownTimer.stop();
    writeConfig(m_previousContents);
    runHyprctl({"reload"});
    m_pendingConfirmation = false;
    m_secondsRemaining = 0;
    m_previousContents.clear();
    emit pendingConfirmationChanged();
    emit secondsRemainingChanged();
    setStatus(tr("Previous display configuration restored"));
    refresh();
}

void Backend::setError(const QString &value)
{
    if (m_error == value)
        return;
    m_error = value;
    emit errorChanged();
}

void Backend::setStatus(const QString &value)
{
    if (m_status == value)
        return;
    m_status = value;
    emit statusChanged();
}

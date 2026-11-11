#include "translator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

Translator::Translator(const QString &languageFile, const QString &catalogDirectory, QObject *parent)
    : QTranslator(parent),
      m_languageFile(languageFile.isEmpty() ? QDir::homePath() + "/.config/omarchy/language" : languageFile),
      m_catalogDirectory(catalogDirectory)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Translator::reload);
    reload();
}

QString Translator::readLanguage(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return "en";
    const QString code = QString::fromUtf8(file.readLine()).trimmed();
    static const QRegularExpression valid("^[a-z]{2,3}(-[A-Za-z]{2})?$");
    return valid.match(code).hasMatch() ? code : "en";
}

QString Translator::translate(const char *, const char *sourceText, const char *, int) const
{
    const QJsonValue value = m_strings.value(QString::fromUtf8(sourceText));
    return value.isString() ? value.toString() : QString();
}

void Translator::reload()
{
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    const QString directory = QFileInfo(m_languageFile).absolutePath();
    if (QFileInfo::exists(directory))
        m_watcher.addPath(directory);
    if (QFileInfo::exists(m_languageFile))
        m_watcher.addPath(m_languageFile);

    const QString language = readLanguage(m_languageFile);
    if (language == m_language)
        return;
    QHash<QString, QJsonValue> strings;
    QStringList codes{"en", language.section('-', 0, 0), language};
    codes.removeDuplicates();
    for (const QString &code : codes) {
        QFile catalog(m_catalogDirectory + "/" + code + ".json");
        if (!catalog.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject object = QJsonDocument::fromJson(catalog.readAll()).object();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (it.value().isString() && !it.value().toString().isEmpty())
                strings.insert(it.key(), it.value());
    }
    m_language = language;
    m_strings = strings;
    const QLocale locale(language);
    QLocale::setDefault(locale);
    QGuiApplication::setLayoutDirection(locale.textDirection());
    emit changed();
}

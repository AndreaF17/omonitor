#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

class Backend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY monitorsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool pendingConfirmation READ pendingConfirmation NOTIFY pendingConfirmationChanged)
    Q_PROPERTY(int secondsRemaining READ secondsRemaining NOTIFY secondsRemainingChanged)

public:
    explicit Backend(QObject *parent = nullptr, const QString &configPath = {},
                     const QString &hyprctl = {}, bool autoRefresh = true);

    QVariantList monitors() const { return m_monitors; }
    QString error() const { return m_error; }
    QString status() const { return m_status; }
    bool pendingConfirmation() const { return m_pendingConfirmation; }
    int secondsRemaining() const { return m_secondsRemaining; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setPosition(int index, int x, int y);
    Q_INVOKABLE void setMode(int index, const QString &mode);
    Q_INVOKABLE void setScale(int index, double scale);
    Q_INVOKABLE void setTransform(int index, int transform);
    Q_INVOKABLE void alignTop(int index, int referenceIndex);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void confirm();
    Q_INVOKABLE void revert();

    bool loadFromJson(const QByteArray &json);
    static QString managedBlock(const QVariantList &monitors);
    static QString replaceManagedBlock(const QString &contents, const QString &block);

signals:
    void monitorsChanged();
    void errorChanged();
    void statusChanged();
    void pendingConfirmationChanged();
    void secondsRemainingChanged();

private:
    QByteArray runHyprctl(const QStringList &arguments, bool *ok = nullptr) const;
    bool writeConfig(const QString &contents);
    void setError(const QString &value);
    void setStatus(const QString &value);
    bool validIndex(int index) const;
    void updateMonitor(int index, const QVariantMap &monitor);
    void startConfirmation();

    QVariantList m_monitors;
    QString m_error;
    QString m_status;
    QString m_configPath;
    QString m_hyprctl;
    QString m_previousContents;
    QTimer m_revertTimer;
    QTimer m_countdownTimer;
    bool m_pendingConfirmation = false;
    int m_secondsRemaining = 0;
};

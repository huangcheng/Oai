#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QPoint>
#include <QString>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    void load();
    void save();

    QPoint windowPosition() const { return m_windowPosition; }
    void setWindowPosition(const QPoint &pos);

    QString language() const { return m_language; }
    void setLanguage(const QString &lang);

    bool autoStart() const { return m_autoStart; }
    void setAutoStart(bool enabled);

    /**
     * Returns the TCP endpoint for IPC.
     * Format: "host:port", e.g. "127.0.0.1:52847"
     */
    QString ipcEndpoint() const { return m_ipcEndpoint; }

    /** Default IPC endpoint (used when config has no override). */
    static QString defaultEndpoint();

signals:
    void languageChanged(const QString &lang);

private:
    QString configFilePath() const;

    QPoint m_windowPosition;
    QString m_language = "en";
    bool m_autoStart = false;
    QString m_ipcEndpoint;
};

#endif // CONFIGMANAGER_H

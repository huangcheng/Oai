#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QSettings>

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

    /** Pack ID of the last-selected character pack, or empty on first run. */
    QString activePackId() const { return m_activePackId; }
    void setActivePackId(const QString &packId);

    /**
     * Returns the TCP endpoint for IPC.
     * Format: "host:port", e.g. "127.0.0.1:52847"
     */
    QString ipcEndpoint() const { return m_ipcEndpoint; }

    /**
     * Extract just the port number from the current endpoint.
     */
    quint16 ipcPort() const;

    /**
     * Set a new IPC port (keeps host as 127.0.0.1).
     * Saves config and emits ipcEndpointChanged if the value changed.
     */
    void setIpcPort(quint16 port);

    /**
     * Returns the UDP endpoint for the version-check / update server.
     * Format: "host:port", e.g. "101.133.144.133:9340". Stored under the
     * `updateServerEndpoint` key in QSettings; falls back to
     * defaultUpdateEndpoint() when unset so production stays operable
     * without forcing every user to write a config file.
     */
    QString updateServerEndpoint() const { return m_updateServerEndpoint; }
    void setUpdateServerEndpoint(const QString &endpoint);

    /** Default IPC endpoint (used when config has no override). */
    static QString defaultEndpoint();

    /** Default update-server endpoint (Aliyun host serving the Erlang UDP daemon). */
    static QString defaultUpdateEndpoint();

signals:
    void languageChanged(const QString &lang);
    void ipcEndpointChanged(const QString &endpoint);
    void updateServerEndpointChanged(const QString &endpoint);

private:
    QSettings m_settings;

    QPoint m_windowPosition;
    QString m_language = "en";
    bool m_autoStart = false;
    QString m_ipcEndpoint;
    QString m_updateServerEndpoint;
    QString m_activePackId;
};

#endif // CONFIGMANAGER_H

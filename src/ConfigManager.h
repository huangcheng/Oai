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
     * Returns the platform-appropriate IPC endpoint:
     *   - Linux/macOS: Unix domain socket path, e.g. "~/.qlippy/qlippy.sock"
     *   - Windows:     Named pipe, e.g. "\\.\pipe\qlippy"
     */
    QString ipcEndpoint() const { return m_ipcEndpoint; }

    /** True when the endpoint is a Windows named pipe, false for Unix socket. */
    bool isNamedPipe() const { return m_isNamedPipe; }

    /** Platform default endpoint (used when config has no override). */
    static QString defaultEndpoint();

signals:
    void languageChanged(const QString &lang);

private:
    QString configFilePath() const;

    QPoint m_windowPosition;
    QString m_language = "en";
    bool m_autoStart = false;
    QString m_ipcEndpoint;
    bool m_isNamedPipe = false;
};

#endif // CONFIGMANAGER_H

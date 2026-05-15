#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QHostAddress>

class QUdpSocket;
class QTimer;
class ConfigManager;

/**
 * @brief Speaks the OAI binary UDP protocol against the update server to
 *        ask "is there a newer version?".
 *
 * Datagram layout (see .claude/skills/oai-server/SKILL.md):
 *   magic 'OAI'\x01 | ver=1 | cmd | seq u16BE | len u16BE | json | crc16-ccitt u16BE
 *
 * Command 1 (CHECK)    — client sends {"current_version","platform"}
 * Command 2 (ANNOUNCE) — server replies {"available", "latest_version"?}
 *
 * The endpoint is read from ConfigManager::updateServerEndpoint(), which
 * defaults to the OAI_DEFAULT_UPDATE_ENDPOINT compiled in via CMake and
 * can be overridden at runtime via QSettings for local-server development
 * without rebuilding.
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(ConfigManager *config, QObject *parent = nullptr);

    /**
     * @param userTriggered Set to true when the user explicitly clicked
     *        "Check for Updates" — so the receiver (SystemTray) knows
     *        whether to surface "no update available" / "check failed"
     *        feedback. Background/auto checks should leave this false
     *        so a quiet network never bothers the user. If a manual
     *        request arrives while an auto check is in flight, the
     *        in-flight response gets upgraded to user-triggered.
     */
    void checkForUpdates(bool userTriggered = false);

    /// True if the most-recent (or in-flight) check originated from a user
    /// action. SystemTray slots query this to decide whether silent results
    /// suppress their notifications.
    bool wasUserTriggered() const { return m_userTriggered; }

    static QString currentVersion();

signals:
    void updateAvailable(const QString &currentVersion, const QString &latestVersion, const QString &downloadUrl);
    void noUpdateAvailable(const QString &currentVersion);
    void checkFailed(const QString &error);

private slots:
    void onReadyRead();
    void onTimeout();

private:
    QByteArray encodeCheck(quint16 seq) const;
    static quint16 crc16ccitt(const QByteArray &data);
    static QString platformTag();

    // Emit the CHECK datagram once the server's IP address is known.
    // Used by both the literal-IP fast path and the QHostInfo callback.
    void sendCheckPacket(const QHostAddress &addr, quint16 port);

    ConfigManager *m_config;
    QUdpSocket *m_socket = nullptr;
    QTimer *m_timeout = nullptr;
    quint16 m_pendingSeq = 0;
    bool m_inFlight = false;
    bool m_userTriggered = false;

    static constexpr int CHECK_TIMEOUT_MS = 5000;
    static constexpr int CMD_CHECK = 1;
    static constexpr int CMD_ANNOUNCE = 2;
};

#endif // UPDATECHECKER_H

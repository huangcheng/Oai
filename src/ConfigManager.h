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
    enum class DisplayMode { Character, Ecg };

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

    /** Current display mode: Character (animated pet) or Ecg (ICU monitor widget). */
    DisplayMode displayMode() const { return m_displayMode; }
    void setDisplayMode(DisplayMode mode);

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

    /** Global shortcut key sequence (e.g. "Ctrl+Shift+O"). */
    QString globalShortcut() const { return m_globalShortcut; }
    void setGlobalShortcut(const QString &shortcut);

    /** Whether the global shortcut is enabled. */
    bool globalShortcutEnabled() const { return m_globalShortcutEnabled; }
    void setGlobalShortcutEnabled(bool enabled);

    /** Whether Gaming Mode (auto-hide when a fullscreen app is active) is enabled. */
    bool gamingModeEnabled() const { return m_gamingModeEnabled; }
    void setGamingModeEnabled(bool enabled);

    /** Whether tip bubbles surface above the pet. Default true. */
    bool tipBubblesEnabled() const { return m_tipBubblesEnabled; }
    void setTipBubblesEnabled(bool enabled);

    /** Whether TTS (Text-to-Speech) is enabled. Default false. */
    bool ttsEnabled() const { return m_ttsEnabled; }
    void setTtsEnabled(bool enabled);

    /** TTS provider base URL (WebSocket endpoint). */
    QString ttsBaseUrl() const { return m_ttsBaseUrl; }
    void setTtsBaseUrl(const QString &url);

    /** TTS API authentication token. */
    QString ttsToken() const { return m_ttsToken; }
    void setTtsToken(const QString &token);

    /** TTS model identifier (e.g., "step-tts-mini", "speech-01"). */
    QString ttsModel() const { return m_ttsModel; }
    void setTtsModel(const QString &model);

    /** TTS language/voice identifier (e.g., "zh-CN", "en-US"). */
    QString ttsLanguage() const { return m_ttsLanguage; }
    void setTtsLanguage(const QString &language);

    /** TTS voice ID (provider-specific, e.g., "cixingnansheng"). */
    QString ttsVoice() const { return m_ttsVoice; }
    void setTtsVoice(const QString &voice);

    /**
     * Returns the UDP endpoint for the version-check / update server.
     * Format: "host:port". Stored under the `updateServerEndpoint` key in
     * QSettings; falls back to defaultUpdateEndpoint() (compiled in from
     * the OAI_DEFAULT_UPDATE_ENDPOINT CMake cache variable) when unset, so
     * production stays operable without forcing every user to write a
     * config file.
     */
    QString updateServerEndpoint() const { return m_updateServerEndpoint; }
    void setUpdateServerEndpoint(const QString &endpoint);

    /** Default IPC endpoint (used when config has no override). */
    static QString defaultEndpoint();

    /** Default update-server endpoint (compiled in from OAI_DEFAULT_UPDATE_ENDPOINT). */
    static QString defaultUpdateEndpoint();

signals:
    void languageChanged(const QString &lang);
    void ipcEndpointChanged(const QString &endpoint);
    void updateServerEndpointChanged(const QString &endpoint);
    void displayModeChanged(DisplayMode mode);
    void globalShortcutChanged(const QString &shortcut);
    void gamingModeEnabledChanged(bool enabled);
    void tipBubblesEnabledChanged(bool enabled);
    void ttsEnabledChanged(bool enabled);
    void ttsBaseUrlChanged(const QString &url);
    void ttsTokenChanged(const QString &token);
    void ttsModelChanged(const QString &model);
    void ttsLanguageChanged(const QString &language);
    void ttsVoiceChanged(const QString &voice);

private:
    QSettings m_settings;

    QPoint m_windowPosition;
    QString m_language = "en";
    bool m_autoStart = false;
    QString m_ipcEndpoint;
    QString m_updateServerEndpoint;
    QString m_activePackId;
    DisplayMode m_displayMode = DisplayMode::Character;
    QString m_globalShortcut = QStringLiteral("Ctrl+Shift+O");
    bool m_globalShortcutEnabled = true;
    bool m_gamingModeEnabled = false;
    bool m_tipBubblesEnabled = true;
    bool m_ttsEnabled = false;
    QString m_ttsBaseUrl;
    QString m_ttsToken;
    QString m_ttsModel;
    QString m_ttsLanguage = QStringLiteral("zh-CN");
    QString m_ttsVoice = QStringLiteral("cixingnansheng");

    /**
     * Reflect m_autoStart into the OS's per-user "launch at login" facility:
     *   - Windows: HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Oai
     *   - macOS:   ~/Library/LaunchAgents/im.cheng.oai.plist (launchd)
     *   - Linux:   ~/.config/autostart/oai.desktop (XDG autostart)
     * On AppImage builds the Linux path uses $APPIMAGE rather than the
     * transient FUSE-mount path returned by applicationFilePath().
     */
    void applyAutoStartToOS(bool enabled);
};

#endif // CONFIGMANAGER_H

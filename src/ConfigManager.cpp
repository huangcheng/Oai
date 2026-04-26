#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>

QString ConfigManager::defaultEndpoint()
{
    return QStringLiteral("127.0.0.1:52847");
}

QString ConfigManager::defaultUpdateEndpoint()
{
    // Aliyun-hosted Erlang/OTP UDP server — see .claude/skills/oai-server/.
    // Override with `updateServerEndpoint=host:port` in QSettings to point
    // at a local server during development.
    return QStringLiteral("101.133.144.133:9340");
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, "Oai", "Oai")
    , m_ipcEndpoint(defaultEndpoint())
    , m_updateServerEndpoint(defaultUpdateEndpoint())
{
    // Layer 1: in-memory defaults (above).
    // Layer 2: portable Oai.ini next to the exe — shipped by the installer
    //          so a built distribution can pre-set updateServerEndpoint /
    //          ipcEndpoint / language without forcing every user to edit
    //          their roaming config. Read-only here; save() never writes
    //          to it (user changes always land in the user-scope file).
    // Layer 3: user-scope ~/AppData/Roaming/Oai/Oai.ini — applied by load()
    //          after construction, wins over portable values.
    const QString portablePath = QCoreApplication::applicationDirPath() + "/Oai.ini";
    if (QFile::exists(portablePath)) {
        QSettings portable(portablePath, QSettings::IniFormat);
        if (const auto v = portable.value("updateServerEndpoint").toString(); !v.isEmpty()) {
            m_updateServerEndpoint = v;
        }
        if (const auto v = portable.value("ipcEndpoint").toString(); !v.isEmpty()) {
            m_ipcEndpoint = v;
        }
        if (const auto v = portable.value("language").toString(); !v.isEmpty()) {
            m_language = v;
        }
        if (portable.contains("autoStart")) {
            m_autoStart = portable.value("autoStart").toBool();
        }
        qDebug() << "ConfigManager: portable defaults loaded from" << portablePath;
    }
}

void ConfigManager::load()
{
    if (!m_settings.contains("language")) {
        qDebug() << "No config file found, using defaults";
        return;
    }

    // Window position
    m_windowPosition.setX(m_settings.value("windowX", 0).toInt());
    m_windowPosition.setY(m_settings.value("windowY", 0).toInt());

    // Language
    m_language = m_settings.value("language", "en").toString();

    // Auto-start
    m_autoStart = m_settings.value("autoStart", false).toBool();
    // Re-apply to the OS on every startup so the registry stays in sync
    // with the INI even after a reinstall to a different path, a manual
    // `reg delete`, or a settings sync from another machine. The call is
    // idempotent — sets/removes the same value either way.
    applyAutoStartToOS(m_autoStart);

    // IPC endpoint
    QString endpoint = m_settings.value("ipcEndpoint").toString();
    if (!endpoint.isEmpty()) {
        m_ipcEndpoint = endpoint;
    }

    // Update-server endpoint (UDP daemon for version checks)
    QString upd = m_settings.value("updateServerEndpoint").toString();
    if (!upd.isEmpty()) {
        m_updateServerEndpoint = upd;
    }

    // Last-selected character pack
    m_activePackId = m_settings.value("activePackId").toString();

    qDebug() << "Config loaded from:" << m_settings.fileName();
}

void ConfigManager::save()
{
    m_settings.setValue("windowX", m_windowPosition.x());
    m_settings.setValue("windowY", m_windowPosition.y());
    m_settings.setValue("language", m_language);
    m_settings.setValue("autoStart", m_autoStart);
    m_settings.setValue("ipcEndpoint", m_ipcEndpoint);
    m_settings.setValue("updateServerEndpoint", m_updateServerEndpoint);
    m_settings.setValue("activePackId", m_activePackId);
    m_settings.sync();
}

void ConfigManager::setWindowPosition(const QPoint &pos)
{
    if (m_windowPosition != pos) {
        m_windowPosition = pos;
        save();
    }
}

void ConfigManager::setLanguage(const QString &lang)
{
    if (m_language != lang) {
        m_language = lang;
        save();
        emit languageChanged(lang);
    }
}

void ConfigManager::setAutoStart(bool enabled)
{
    if (m_autoStart != enabled) {
        m_autoStart = enabled;
        save();
        applyAutoStartToOS(enabled);
    }
}

void ConfigManager::applyAutoStartToOS(bool enabled)
{
#ifdef Q_OS_WIN
    // Per-user "launch at login" lives in
    //   HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
    // Setting a value there names a program that Windows runs at logon
    // for the current user. Removing the value disables it. No admin
    // rights needed because this is HKCU, not HKLM.
    QSettings runKey(
        QStringLiteral(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)"),
        QSettings::NativeFormat);
    static constexpr QLatin1String kRunValue("Oai");

    if (enabled) {
        // Quote the path: a default install lands in C:\Program Files\Oai\
        // (space in the parent), and Windows shell parses the Run value as
        // a command line, splitting on unquoted whitespace.
        const QString exe = QDir::toNativeSeparators(
            QCoreApplication::applicationFilePath());
        runKey.setValue(kRunValue, QStringLiteral("\"%1\"").arg(exe));
    } else {
        runKey.remove(kRunValue);
    }
    runKey.sync();
#else
    Q_UNUSED(enabled);
    // TODO(macOS):  ~/Library/LaunchAgents/im.cheng.oai.plist  (launchd)
    // TODO(Linux):  ~/.config/autostart/oai.desktop            (XDG)
#endif
}

void ConfigManager::setActivePackId(const QString &packId)
{
    if (m_activePackId != packId) {
        m_activePackId = packId;
        save();
    }
}

quint16 ConfigManager::ipcPort() const
{
    int colonPos = m_ipcEndpoint.lastIndexOf(':');
    if (colonPos > 0) {
        return static_cast<quint16>(m_ipcEndpoint.mid(colonPos + 1).toUInt());
    }
    return 52847;
}

void ConfigManager::setIpcPort(quint16 port)
{
    const QString newEndpoint = QStringLiteral("127.0.0.1:") + QString::number(port);
    if (m_ipcEndpoint != newEndpoint) {
        m_ipcEndpoint = newEndpoint;
        save();
        emit ipcEndpointChanged(m_ipcEndpoint);
    }
}

void ConfigManager::setUpdateServerEndpoint(const QString &endpoint)
{
    if (m_updateServerEndpoint != endpoint) {
        m_updateServerEndpoint = endpoint;
        save();
        emit updateServerEndpointChanged(m_updateServerEndpoint);
    }
}

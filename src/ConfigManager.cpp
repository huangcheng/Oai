#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QDebug>

QString ConfigManager::defaultEndpoint()
{
    return QStringLiteral("127.0.0.1:52847");
}

QString ConfigManager::defaultUpdateEndpoint()
{
    // Default host:port comes from OAI_DEFAULT_UPDATE_ENDPOINT, baked in at
    // CMake-configure time (see CMakeLists.txt). Override at runtime via the
    // `updateServerEndpoint` key in QSettings to point at a local server
    // during development without rebuilding.
    return QStringLiteral(OAI_DEFAULT_UPDATE_ENDPOINT);
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope,
                 QCoreApplication::organizationName().isEmpty()
                     ? QStringLiteral("Oai")
                     : QCoreApplication::organizationName(),
                 QCoreApplication::applicationName().isEmpty()
                     ? QStringLiteral("Oai")
                     : QCoreApplication::applicationName())
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

    // Display mode (with migration from old ecgEnabled key)
    if (m_settings.contains("displayMode")) {
        const QString modeStr = m_settings.value("displayMode").toString();
        m_displayMode = (modeStr == QStringLiteral("ecg"))
                        ? DisplayMode::Ecg
                        : DisplayMode::Character;
    } else if (m_settings.contains("ecgEnabled")) {
        // Migrate from old ecgEnabled boolean
        m_displayMode = m_settings.value("ecgEnabled", false).toBool()
                        ? DisplayMode::Ecg
                        : DisplayMode::Character;
        // Write new key and remove old one; persist immediately
        m_settings.setValue("displayMode",
                            m_displayMode == DisplayMode::Ecg
                                ? QStringLiteral("ecg")
                                : QStringLiteral("character"));
        m_settings.remove("ecgEnabled");
        m_settings.sync();
    } else {
        m_displayMode = DisplayMode::Character;
    }

    m_globalShortcut = m_settings.value("globalShortcut", m_globalShortcut).toString();
    m_globalShortcutEnabled = m_settings.value("globalShortcutEnabled", m_globalShortcutEnabled).toBool();
    m_mouseTrackingEnabled = m_settings.value("mouseTrackingEnabled", m_mouseTrackingEnabled).toBool();

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
    m_settings.setValue("displayMode",
                        m_displayMode == DisplayMode::Ecg
                            ? QStringLiteral("ecg")
                            : QStringLiteral("character"));
    m_settings.setValue("globalShortcut", m_globalShortcut);
    m_settings.setValue("globalShortcutEnabled", m_globalShortcutEnabled);
    m_settings.setValue("mouseTrackingEnabled", m_mouseTrackingEnabled);
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
#if defined(Q_OS_WIN)
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

#elif defined(Q_OS_MAC)
    // launchd LaunchAgent at ~/Library/LaunchAgents/im.cheng.oai.plist.
    // Loaded automatically at next login because launchd scans that dir
    // when GUI session starts. To activate immediately on toggle one
    // would also `launchctl bootstrap gui/<uid> <plist>`, but launching
    // the app mid-toggle is jarring; we accept "next login" semantics
    // (matches Windows behaviour above).
    const QString plistPath = QDir::homePath()
        + QStringLiteral("/Library/LaunchAgents/im.cheng.oai.plist");

    if (enabled) {
        // Point at the bundle's MacOS executable, NOT the .app dir —
        // launchd does not "open" .app bundles, it execs binaries.
        const QString exe = QCoreApplication::applicationFilePath();
        QDir().mkpath(QFileInfo(plistPath).absolutePath());
        QFile f(plistPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&f);
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                   "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                << "<plist version=\"1.0\">\n"
                << "<dict>\n"
                << "    <key>Label</key>\n"
                << "    <string>im.cheng.oai</string>\n"
                << "    <key>ProgramArguments</key>\n"
                << "    <array>\n"
                << "        <string>" << exe.toHtmlEscaped() << "</string>\n"
                << "    </array>\n"
                << "    <key>RunAtLoad</key>\n"
                << "    <true/>\n"
                << "</dict>\n"
                << "</plist>\n";
        } else {
            qWarning() << "auto-start: cannot write" << plistPath << f.errorString();
        }
    } else {
        QFile::remove(plistPath);
    }

#elif defined(Q_OS_LINUX)
    // XDG autostart spec: ~/.config/autostart/<name>.desktop is sourced
    // by GNOME, KDE, XFCE, etc. when the user logs into a graphical
    // session. GenericConfigLocation gives ~/.config (the app-specific
    // ConfigLocation would give ~/.config/Oai, which the spec doesn't
    // scan).
    const QString desktopPath = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/oai.desktop");

    if (enabled) {
        // For AppImage installs, applicationFilePath() returns a
        // transient FUSE-mount path (/tmp/.mount_xxx/usr/bin/Oai) that
        // changes every run. The AppImage runtime sets $APPIMAGE to the
        // stable .AppImage path — prefer that when present.
        const QByteArray appImage = qgetenv("APPIMAGE");
        const QString exe = appImage.isEmpty()
            ? QCoreApplication::applicationFilePath()
            : QString::fromLocal8Bit(appImage);

        QDir().mkpath(QFileInfo(desktopPath).absolutePath());
        QFile f(desktopPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&f);
            out << "[Desktop Entry]\n"
                << "Type=Application\n"
                << "Name=Oai\n"
                << "Exec=\"" << exe << "\"\n"
                << "Icon=oai\n"
                << "Terminal=false\n"
                << "X-GNOME-Autostart-enabled=true\n";
        } else {
            qWarning() << "auto-start: cannot write" << desktopPath << f.errorString();
        }
    } else {
        QFile::remove(desktopPath);
    }

#else
    Q_UNUSED(enabled);
    // Other Unix variants (BSD, Haiku, ...) — no auto-start handling.
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

void ConfigManager::setDisplayMode(DisplayMode mode)
{
    if (m_displayMode != mode) {
        m_displayMode = mode;
        save();
        emit displayModeChanged(mode);
    }
}

void ConfigManager::setGlobalShortcut(const QString &shortcut)
{
    if (m_globalShortcut == shortcut) return;
    m_globalShortcut = shortcut;
    save();
    emit globalShortcutChanged(shortcut);
}

void ConfigManager::setGlobalShortcutEnabled(bool enabled)
{
    if (m_globalShortcutEnabled == enabled) return;
    m_globalShortcutEnabled = enabled;
    save();
}

void ConfigManager::setMouseTrackingEnabled(bool enabled)
{
    if (m_mouseTrackingEnabled == enabled) return;
    m_mouseTrackingEnabled = enabled;
    save();
    emit mouseTrackingEnabledChanged(enabled);
}

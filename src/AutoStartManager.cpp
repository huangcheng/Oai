#include "AutoStartManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#if defined(Q_OS_WIN)
#  include <QSettings>
#endif

namespace AutoStartManager {

#if defined(Q_OS_WIN)

void setEnabled(bool enabled)
{
    // Per-user "launch at login" lives in
    //   HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
    // Setting a value there names a program that Windows runs at logon
    // for the current user. Removing the value disables it. No admin
    // rights needed because this is HKCU, not HKLM.
    QSettings runKey(
        QStringLiteral(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)"),
        QSettings::NativeFormat);
    static constexpr QLatin1String kRunValue("Seelie");

    if (enabled) {
        // Quote the path: a default install lands in C:\Program Files\Seelie\
        // (space in the parent), and Windows shell parses the Run value as
        // a command line, splitting on unquoted whitespace.
        const QString exe = QDir::toNativeSeparators(
            QCoreApplication::applicationFilePath());
        runKey.setValue(kRunValue, QStringLiteral("\"%1\"").arg(exe));
    } else {
        runKey.remove(kRunValue);
    }
    runKey.sync();
}

bool isEnabled()
{
    QSettings runKey(
        QStringLiteral(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)"),
        QSettings::NativeFormat);
    return runKey.contains(QStringLiteral("Seelie"));
}

#elif defined(Q_OS_MAC)

namespace {

// Proper XML escape — Qt's toHtmlEscaped() turns ' into &#39;, which
// happens to be valid XML, but the named-entity set differs in edge
// cases. Be explicit so the plist parser can never misread an executable
// path containing &, <, >, ", or '.
QString xmlEscape(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        switch (c.unicode()) {
            case '&':  out += QStringLiteral("&amp;");  break;
            case '<':  out += QStringLiteral("&lt;");   break;
            case '>':  out += QStringLiteral("&gt;");   break;
            case '"':  out += QStringLiteral("&quot;"); break;
            case '\'': out += QStringLiteral("&apos;"); break;
            default:   out += c;
        }
    }
    return out;
}

} // namespace

void setEnabled(bool enabled)
{
    // launchd LaunchAgent at ~/Library/LaunchAgents/im.cheng.seelie.plist.
    // Loaded automatically at next login because launchd scans that dir
    // when GUI session starts. To activate immediately on toggle one
    // would also `launchctl bootstrap gui/<uid> <plist>`, but launching
    // the app mid-toggle is jarring; we accept "next login" semantics
    // (matches Windows behaviour).
    const QString plistPath = QDir::homePath()
        + QStringLiteral("/Library/LaunchAgents/im.cheng.seelie.plist");

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
                << "    <string>im.cheng.seelie</string>\n"
                << "    <key>ProgramArguments</key>\n"
                << "    <array>\n"
                << "        <string>" << xmlEscape(exe) << "</string>\n"
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
}

bool isEnabled()
{
    const QString plistPath = QDir::homePath()
        + QStringLiteral("/Library/LaunchAgents/im.cheng.seelie.plist");
    return QFile::exists(plistPath);
}

#elif defined(Q_OS_LINUX)

void setEnabled(bool enabled)
{
    // XDG autostart spec: ~/.config/autostart/<name>.desktop is sourced
    // by GNOME, KDE, XFCE, etc. when the user logs into a graphical
    // session. GenericConfigLocation gives ~/.config (the app-specific
    // ConfigLocation would give ~/.config/Seelie, which the spec doesn't
    // scan).
    const QString desktopPath = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/seelie.desktop");

    if (enabled) {
        // For AppImage installs, applicationFilePath() returns a
        // transient FUSE-mount path (/tmp/.mount_xxx/usr/bin/Seelie) that
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
                << "Name=Seelie\n"
                << "Exec=\"" << exe << "\"\n"
                << "Icon=seelie\n"
                << "Terminal=false\n"
                << "X-GNOME-Autostart-enabled=true\n";
        } else {
            qWarning() << "auto-start: cannot write" << desktopPath << f.errorString();
        }
    } else {
        QFile::remove(desktopPath);
    }
}

bool isEnabled()
{
    const QString desktopPath = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/seelie.desktop");
    return QFile::exists(desktopPath);
}

#else

void setEnabled(bool /*enabled*/)
{
    // Other Unix variants (BSD, Haiku, ...) — no auto-start handling.
}

bool isEnabled()
{
    return false;
}

#endif

} // namespace AutoStartManager

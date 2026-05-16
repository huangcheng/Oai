#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include "AutoStartManager.h"

// Tests targeted at the per-OS file/registry side effects of
// AutoStartManager::setEnabled. We don't test the registry on macOS / Linux,
// or the plist on Windows — each #if branch only verifies its own platform.
class TestAutoStartManager : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

#if defined(Q_OS_MAC)
    void macOsPlistRoundTrip();
#elif defined(Q_OS_LINUX)
    void linuxDesktopFileRoundTrip();
#elif defined(Q_OS_WIN)
    // Skipped under unit tests — would touch the user's actual Run key.
    void windowsToggleNoOp() { QSKIP("Touches HKCU registry; covered manually"); }
#endif
};

void TestAutoStartManager::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("SeelieTestAutoStart");
}

#if defined(Q_OS_MAC)
void TestAutoStartManager::macOsPlistRoundTrip()
{
    const QString plistPath = QDir::homePath()
        + QStringLiteral("/Library/LaunchAgents/im.cheng.seelie.plist");
    // Ensure baseline is clean — if a previous test failed mid-flight we
    // don't want to assert against stale state.
    QFile::remove(plistPath);
    QVERIFY(!QFile::exists(plistPath));
    QVERIFY2(!AutoStartManager::isEnabled(),
             "isEnabled() must return false when plist absent");

    AutoStartManager::setEnabled(true);
    QVERIFY2(QFile::exists(plistPath), "setEnabled(true) must create the plist");
    QVERIFY2(AutoStartManager::isEnabled(),
             "isEnabled() must return true after setEnabled(true)");

    QFile f(plistPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(f.readAll());
    f.close();

    QVERIFY(contents.contains(QStringLiteral("<key>Label</key>")));
    QVERIFY(contents.contains(QStringLiteral("<string>im.cheng.seelie</string>")));
    QVERIFY(contents.contains(QStringLiteral("<key>RunAtLoad</key>")));

    AutoStartManager::setEnabled(false);
    QVERIFY2(!QFile::exists(plistPath), "setEnabled(false) must remove the plist");
    QVERIFY2(!AutoStartManager::isEnabled(),
             "isEnabled() must return false after setEnabled(false)");
}
#endif

#if defined(Q_OS_LINUX)
void TestAutoStartManager::linuxDesktopFileRoundTrip()
{
    const QString desktopPath = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/seelie.desktop");
    QFile::remove(desktopPath);
    QVERIFY(!QFile::exists(desktopPath));
    QVERIFY2(!AutoStartManager::isEnabled(),
             "isEnabled() must return false when .desktop absent");

    AutoStartManager::setEnabled(true);
    QVERIFY2(QFile::exists(desktopPath), "setEnabled(true) must create the desktop file");
    QVERIFY2(AutoStartManager::isEnabled(),
             "isEnabled() must return true after setEnabled(true)");

    QFile f(desktopPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(f.readAll());
    f.close();
    QVERIFY(contents.contains(QStringLiteral("[Desktop Entry]")));
    QVERIFY(contents.contains(QStringLiteral("Type=Application")));
    QVERIFY(contents.contains(QStringLiteral("Name=Seelie")));

    AutoStartManager::setEnabled(false);
    QVERIFY2(!QFile::exists(desktopPath), "setEnabled(false) must remove the desktop file");
    QVERIFY2(!AutoStartManager::isEnabled(),
             "isEnabled() must return false after setEnabled(false)");
}
#endif

QTEST_MAIN(TestAutoStartManager)
#include "test_autostart_manager.moc"

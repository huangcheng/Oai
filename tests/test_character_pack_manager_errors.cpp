#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "CharacterPackManager.h"

// Tests for the H17 lastError() reporting path. We don't synthesize a real
// .opk archive (that's covered by manual / integration tests) — we drive
// installPack with bogus inputs and assert the error message is populated
// with a meaningful reason.
class TestCharacterPackManagerErrors : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void installNonexistentFile();
    void uninstallUnknownPack();
};

void TestCharacterPackManagerErrors::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("SeelieTestPackErrors");
}

void TestCharacterPackManagerErrors::installNonexistentFile()
{
    CharacterPackManager mgr;
    QTemporaryDir userDir;
    QVERIFY(userDir.isValid());
    mgr.initialize({}, userDir.path(), {});

    const QString notAnArchive = userDir.path() + "/does-not-exist.opk";
    QVERIFY(!mgr.installPack(notAnArchive));
    QVERIFY2(!mgr.lastError().isEmpty(),
             "installPack must populate lastError on failure");
}

void TestCharacterPackManagerErrors::uninstallUnknownPack()
{
    CharacterPackManager mgr;
    QTemporaryDir userDir;
    QVERIFY(userDir.isValid());
    mgr.initialize({}, userDir.path(), {});

    QVERIFY(!mgr.uninstallPack("never-installed"));
    QVERIFY2(!mgr.lastError().isEmpty(),
             "uninstallPack must populate lastError on failure");
    QVERIFY2(mgr.lastError().contains("never-installed"),
             "error should mention the offending pack id");
}

QTEST_MAIN(TestCharacterPackManagerErrors)
#include "test_character_pack_manager_errors.moc"

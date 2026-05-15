#include <QtTest/QtTest>
#include <QApplication>
#include <QWidget>

#include "PlatformWindow.h"

// Smoke tests for PlatformWindow. On non-Windows builds the helpers are
// pure no-ops; we just want to verify they don't crash on null inputs and
// don't mutate widget state in unexpected ways.
class TestPlatformWindow : public QObject
{
    Q_OBJECT
private slots:
    void nullWidgetIsSafe();
    void widgetWithoutHwndIsSafe();
};

void TestPlatformWindow::nullWidgetIsSafe()
{
    // Both helpers must early-return on nullptr — otherwise a null deref.
    PlatformWindow::applyDwmFramelessAttributes(nullptr);
    PlatformWindow::refreshComposition(nullptr);
}

void TestPlatformWindow::widgetWithoutHwndIsSafe()
{
    // A widget that has never been shown has no native handle yet.
    // The helpers should fail-soft (Windows: winId() returns 0 → early
    // return; non-Windows: no-op anyway).
    QWidget w;
    PlatformWindow::applyDwmFramelessAttributes(&w);
    PlatformWindow::refreshComposition(&w);
    // No assertion — if we reach this line the helpers didn't crash.
    QVERIFY(true);
}

QTEST_MAIN(TestPlatformWindow)
#include "test_platform_window.moc"

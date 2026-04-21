#include "SystemTray.h"
#include "mainwindow.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QPainter>
#include <QDebug>

SystemTray::SystemTray(QWidget *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setToolTip(tr("Qlippy Desktop Pet"));

    // Use the application icon for the tray
    QIcon icon = qApp->windowIcon();
    if (icon.isNull()) {
        // Create a simple colored circle as fallback
        QPixmap pixmap(64, 64);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(0, 120, 215)); // Windows accent blue
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(8, 8, 48, 48);
        icon = QIcon(pixmap);
    }
    m_trayIcon->setIcon(icon);

    setupMenu();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &SystemTray::onActivated);
}

SystemTray::~SystemTray() = default;

void SystemTray::show()
{
    m_trayIcon->show();
}

void SystemTray::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Toggle window visibility on click
        if (m_mainWindow) {
            if (m_mainWindow->isVisible()) {
                m_mainWindow->hide();
            } else {
                m_mainWindow->show();
                m_mainWindow->raise();
            }
        }
    }
}

void SystemTray::setupMenu()
{
    m_trayMenu = new QMenu();

    m_toggleAction = m_trayMenu->addAction(tr("Show/Hide"));
    connect(m_toggleAction, &QAction::triggered, this, [this]() {
        if (m_mainWindow) {
            if (m_mainWindow->isVisible()) {
                m_mainWindow->hide();
            } else {
                m_mainWindow->show();
                m_mainWindow->raise();
            }
        }
    });

    m_trayMenu->addSeparator();

    m_quitAction = m_trayMenu->addAction(tr("Quit"));
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(m_trayMenu);
}

void SystemTray::retranslateUi()
{
    m_trayIcon->setToolTip(tr("Qlippy Desktop Pet"));
    if (m_toggleAction) {
        m_toggleAction->setText(tr("Show/Hide"));
    }
    if (m_quitAction) {
        m_quitAction->setText(tr("Quit"));
    }
}

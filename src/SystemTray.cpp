#include "SystemTray.h"
#include "mainwindow.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QPixmap>
#include <QImage>

SystemTray::SystemTray(QWidget *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setToolTip(tr("Qlippy Desktop Pet"));

    // Use the application icon scaled for the system tray
    QIcon appIcon = qApp->windowIcon();
    if (!appIcon.isNull()) {
        // macOS menu bar expects ~22px icons; scale from app icon
        QPixmap trayPix = appIcon.pixmap(128, 128)
                              .scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QIcon trayIcon;
        trayIcon.addPixmap(trayPix);
        m_trayIcon->setIcon(trayIcon);
    } else {
        // Fallback: simple colored circle
        QPixmap pixmap(64, 64);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(0, 120, 215));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(8, 8, 48, 48);
        m_trayIcon->setIcon(QIcon(pixmap));
    }

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

#include "SystemTray.h"
#include "mainwindow.h"
#include "CharacterPackManager.h"
#include "CharacterPack.h"
#include "UpdateChecker.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
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
    m_trayIcon->setToolTip(tr("Oai Desktop Pet"));

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
    // Platform-specific font size: macOS uses 13, Windows uses 10
    int menuFontSize = 10;
#ifdef Q_OS_MAC
    menuFontSize = 13;
#endif
    QFont menuFont("HarmonyOS Sans SC", menuFontSize);
    menuFont.setStyleStrategy(QFont::PreferAntialias);

    m_trayMenu = new QMenu();
    m_trayMenu->setFont(menuFont);

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

    // Pack submenu
    m_packMenu = m_trayMenu->addMenu(tr("Pet"));
    m_packMenu->setFont(menuFont);

    m_trayMenu->addSeparator();

    // Check for updates
    m_checkUpdateAction = m_trayMenu->addAction(tr("Check for Updates"));
    connect(m_checkUpdateAction, &QAction::triggered, this, [this]() {
        if (m_updateChecker) {
            m_updateChecker->checkForUpdates();
        }
    });

    m_trayMenu->addSeparator();

    m_quitAction = m_trayMenu->addAction(tr("Quit"));
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(m_trayMenu);
}

void SystemTray::setCharacterPackManager(CharacterPackManager *manager)
{
    m_packManager = manager;
    if (m_packManager) {
        connect(m_packManager, &CharacterPackManager::packListChanged,
                this, &SystemTray::refreshPackMenu);
        refreshPackMenu();
    }
}

void SystemTray::setUpdateChecker(UpdateChecker *checker)
{
    m_updateChecker = checker;
    if (m_updateChecker) {
        connect(m_updateChecker, &UpdateChecker::updateAvailable,
                this, &SystemTray::onUpdateAvailable);
        connect(m_updateChecker, &UpdateChecker::noUpdateAvailable,
                this, &SystemTray::onNoUpdateAvailable);
        connect(m_updateChecker, &UpdateChecker::checkFailed,
                this, &SystemTray::onUpdateCheckFailed);
    }
}

void SystemTray::onUpdateAvailable(const QString &current, const QString &latest, const QString &url)
{
    Q_UNUSED(url);
    m_trayIcon->showMessage(
        tr("Update Available"),
        tr("Version %1 is available (current: %2)").arg(latest, current),
        QSystemTrayIcon::Information,
        5000
    );
}

void SystemTray::onNoUpdateAvailable(const QString &current)
{
    m_trayIcon->showMessage(
        tr("No Updates"),
        tr("You are running the latest version (%1)").arg(current),
        QSystemTrayIcon::Information,
        3000
    );
}

void SystemTray::onUpdateCheckFailed(const QString &error)
{
    m_trayIcon->showMessage(
        tr("Update Check Failed"),
        tr("Could not check for updates: %1").arg(error),
        QSystemTrayIcon::Warning,
        5000
    );
}

void SystemTray::onPackActionTriggered()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action || !m_packManager) {
        return;
    }

    QString packId = action->data().toString();
    if (!packId.isEmpty()) {
        m_packManager->switchPack(packId);
    }
}

void SystemTray::refreshPackMenu()
{
    if (!m_packMenu) {
        qDebug() << "SystemTray::refreshPackMenu — no packMenu";
        return;
    }

    m_packMenu->clear();
    delete m_packActionGroup;
    m_packActionGroup = new QActionGroup(this);
    m_packActionGroup->setExclusive(true);

    if (!m_packManager) {
        qDebug() << "SystemTray::refreshPackMenu — no packManager";
        return;
    }

    const auto packs = m_packManager->availablePacks();
    const QString activeId = m_packManager->activePackId();
    const QString locale = m_packManager->activeLocale();

    // Partition by source so the submenu doesn't grow past one screen-height
    // (Windows QMenu silently scrolls when too long; users miss most items).
    // Heuristic: anything imported via scripts/import_live2d.py carries an
    // author string starting with "Imported from github.com/" — treat those
    // as a separate "Azur Lane" bucket. Everything else is "Originals".
    QVector<CharacterPackManager::PackInfo> originals, imported;
    for (const auto &pack : packs) {
        if (pack.author.startsWith(QStringLiteral("Imported from github.com/"))) {
            imported.append(pack);
        } else {
            originals.append(pack);
        }
    }

    auto addToSubmenu = [this, &activeId, &locale](QMenu *menu,
                                                    const QVector<CharacterPackManager::PackInfo> &list) {
        for (const auto &pack : list) {
            QAction *action = menu->addAction(pack.displayName(locale));
            action->setData(pack.id);
            action->setCheckable(true);
            action->setChecked(pack.id == activeId);
            m_packActionGroup->addAction(action);
            connect(action, &QAction::triggered, this, &SystemTray::onPackActionTriggered);
        }
    };

    if (!originals.isEmpty()) {
        QMenu *originalsMenu = m_packMenu->addMenu(tr("Originals"));
        originalsMenu->setFont(m_packMenu->font());
        addToSubmenu(originalsMenu, originals);
    }
    if (!imported.isEmpty()) {
        QMenu *importedMenu = m_packMenu->addMenu(tr("Azur Lane"));
        importedMenu->setFont(m_packMenu->font());
        addToSubmenu(importedMenu, imported);
    }

    qDebug() << "SystemTray::refreshPackMenu — populated"
             << originals.size() << "Originals +"
             << imported.size() << "Azur Lane";
}

void SystemTray::retranslateUi()
{
    m_trayIcon->setToolTip(tr("Oai Desktop Pet"));
    if (m_toggleAction) {
        m_toggleAction->setText(tr("Show/Hide"));
    }
    if (m_packMenu) {
        m_packMenu->setTitle(tr("Pet"));
    }
    if (m_quitAction) {
        m_quitAction->setText(tr("Quit"));
    }
    // Pack labels can switch between English/Chinese on locale change.
    refreshPackMenu();
}

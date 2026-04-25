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
            // Pass true so the response surfaces feedback whether or not
            // an update is actually available — user clicked, user gets
            // an answer.
            m_updateChecker->checkForUpdates(true);
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
    // Always notify on update-available — the whole point of the auto
    // check is to surface this. Manual checks also report it.
    m_trayIcon->showMessage(
        tr("Update Available"),
        tr("Version %1 is available (current: %2)").arg(latest, current),
        QSystemTrayIcon::Information,
        5000
    );
}

void SystemTray::onNoUpdateAvailable(const QString &current)
{
    // Only feedback the user if THEY asked. The 5s post-launch auto check
    // stays silent on the happy path so we don't pop a balloon every time
    // the app starts up on the latest version.
    if (!m_updateChecker || !m_updateChecker->wasUserTriggered()) {
        return;
    }
    m_trayIcon->showMessage(
        tr("No Updates"),
        tr("You are running the latest version (%1)").arg(current),
        QSystemTrayIcon::Information,
        3000
    );
}

void SystemTray::onUpdateCheckFailed(const QString &error)
{
    // Same rationale as onNoUpdateAvailable — auto checks stay silent on
    // failure (network is flaky, no point bothering the user). Manual
    // checks always surface the error so the user knows their click hit
    // a wall.
    if (!m_updateChecker || !m_updateChecker->wasUserTriggered()) {
        qDebug() << "UpdateChecker: silent failure (auto check):" << error;
        return;
    }
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

// Fixed display order for known categories; unknown ones land at the end
// in their raw-id form. QT_TR_NOOP marks the labels for lupdate without
// running tr() at static-init time.
static const struct {
    const char *id;
    const char *labelEn;   // QT_TR_NOOP'd
} kCategoryOrder[] = {
    { "originals",       QT_TR_NOOP("Standalone") },
    { "azur_lane",       QT_TR_NOOP("Azur Lane") },
    { "girls_frontline", QT_TR_NOOP("Girls' Frontline") },
    { "idol_dimension",  QT_TR_NOOP("Idol Dimension") },
    { "konosuba",        QT_TR_NOOP("Konosuba") },
    { "live2d_samples",  QT_TR_NOOP("Live2D Samples") },
};

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

    // Group packs by category (parser already supplied a default for legacy
    // manifests via author-string heuristics, so `category` is always set).
    QMap<QString, QVector<CharacterPackManager::PackInfo>> grouped;
    for (const auto &pack : packs) {
        const QString cat = pack.category.isEmpty()
                                ? QStringLiteral("originals") : pack.category;
        grouped[cat].append(pack);
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

    QSet<QString> seen;
    for (const auto &c : kCategoryOrder) {
        const QString id = QString::fromLatin1(c.id);
        if (!grouped.contains(id)) continue;
        QMenu *sub = m_packMenu->addMenu(tr(c.labelEn));
        sub->setFont(m_packMenu->font());
        addToSubmenu(sub, grouped[id]);
        seen.insert(id);
    }
    // Future-proof: any category we haven't enumerated lands at the end
    // with the raw id as the menu label.
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        if (seen.contains(it.key())) continue;
        QMenu *sub = m_packMenu->addMenu(it.key());
        sub->setFont(m_packMenu->font());
        addToSubmenu(sub, it.value());
    }

    qDebug() << "SystemTray::refreshPackMenu — populated"
             << packs.size() << "packs across" << grouped.size() << "categories";
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

#include "mainwindow.h"
#include "ConfigManager.h"
#include "IpcServer.h"
#include "EventRouter.h"
#include "SpriteAnimationEngine.h"
#include "LottieAnimationEngine.h"
#ifdef OAI_LIVE2D_SUPPORT
#include "Live2DAnimationEngine.h"
#endif
#include "LottieEffectOverlay.h"
#include "CharacterPackManager.h"
#include "CharacterPack.h"
#include "TipBubbleWidget.h"
#include "TipsEngine.h"
#include "SystemTray.h"
#include "UpdateChecker.h"

#include <QApplication>
#include <QLocale>
#include <QLockFile>
#include <QTranslator>
#include <QDir>
#include <QStandardPaths>
#include <QScreen>
#include <QDebug>
#include <QFontDatabase>
#include <QTimer>

static QString configDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/Oai";
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Oai");
    a.setOrganizationName("Oai");
    a.setWindowIcon(QIcon(":/icons/clippy.png"));
    a.setQuitOnLastWindowClosed(false); // system tray keeps it alive

    // --- Single instance enforcement -----------------------------------------
    const QString lockDir = configDir();
    QDir().mkpath(lockDir);
    QLockFile lockFile(lockDir + "/Oai.lock");
    lockFile.setStaleLockTime(30000); // 30s stale timeout
    if (!lockFile.tryLock(100)) {
        qInfo() << "Oai is already running. Bringing existing instance to front.";
        return 0;
    }

    // --- Config --------------------------------------------------------------
    ConfigManager config;
    config.load();

    // --- Translations --------------------------------------------------------
    QTranslator translator;
    QString lang = config.language();
    if (!lang.isEmpty() && lang != "en") {
        const QString baseName = "Oai_" + lang;
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
        }
    } else {
        // Fall back to system locale
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = "Oai_" + QLocale(locale).name();
            if (translator.load(":/i18n/" + baseName)) {
                a.installTranslator(&translator);
                break;
            }
        }
    }

    // --- Locate assets directory ----------------------------------------------
    // Searches upward from the executable to find the assets folder,
    // handling macOS app bundles, various build dir layouts, and source-tree runs.
    auto findAssetsDir = []() -> QString {
        QDir dir(QApplication::applicationDirPath());
        for (int i = 0; i < 6; ++i) {
            QString candidate = dir.absoluteFilePath("assets");
            if (QFile::exists(candidate + "/map.png")) {
                return candidate;
            }
            if (!dir.cdUp()) break;
        }
        return QString();
    };

    const QString assetsDir = findAssetsDir();
    if (!assetsDir.isEmpty()) {
        qDebug() << "Assets loaded from:" << assetsDir;
    } else {
        qWarning() << "Assets not found. Searched from:" << QApplication::applicationDirPath();
    }

    // --- Font loading (HarmonyOS Sans SC) ------------------------------------
    // Load from filesystem (assets/fonts/) for cross-platform consistency.
    // Must run before MainWindow creation so QApplication::setFont() propagates.
    {
        const QStringList fontFiles = {
            (!assetsDir.isEmpty()) ? assetsDir + "/fonts/HarmonyOS_Sans_SC_Bold.ttf" : QString(),
            (!assetsDir.isEmpty()) ? assetsDir + "/fonts/HarmonyOS_Sans_SC_Regular.ttf" : QString(),
        };
        QString registeredFamily;
        for (const QString &path : fontFiles) {
            if (path.isEmpty() || !QFile::exists(path)) continue;
            int id = QFontDatabase::addApplicationFont(path);
            if (id >= 0) {
                QStringList families = QFontDatabase::applicationFontFamilies(id);
                if (!families.isEmpty() && registeredFamily.isEmpty()) {
                    registeredFamily = families.first();
                    qDebug() << "Font loaded:" << path << "→" << registeredFamily;
                }
            } else {
                qWarning() << "Failed to load font:" << path;
            }
        }
        if (!registeredFamily.isEmpty()) {
            QFont appFont(registeredFamily, 9);
            appFont.setStyleStrategy(QFont::PreferAntialias);
            QApplication::setFont(appFont);
            qDebug() << "Global app font set to:" << registeredFamily;
        } else {
            qWarning() << "HarmonyOS Sans SC not loaded — using system default font";
        }
    }

    // --- Main window ---------------------------------------------------------
    MainWindow w(&config, &translator);

    // --- Sprite pack manager -------------------------------------------------
    CharacterPackManager packManager;
    const QString builtInPacksDir = assetsDir + "/packs";
    const QString userPacksDir = configDir() + "/packs";
    packManager.initialize(builtInPacksDir, userPacksDir);
    w.setCharacterPackManager(&packManager);

    // --- Position window ----------------------------------------------------
    // Clamp to screen bounds to handle: resolution changes, disconnected monitors,
    // or window size changes from sprite pack loading.
    const QSize winSize = w.size();
    const int posMargin = 8;

    // Helper: clamp a position so the entire window fits inside a screen rect
    auto clampedPos = [&](const QPoint &pos, const QRect &geo) -> QPoint {
        int x = qBound(geo.left() + posMargin, pos.x(), geo.right() - winSize.width() - posMargin);
        int y = qBound(geo.top() + posMargin, pos.y(), geo.bottom() - winSize.height() - posMargin);
        return QPoint(x, y);
    };

    // Helper: find the screen whose geometry has the largest overlap with a rect
    auto bestScreenFor = [&](const QRect &rect) -> const QScreen * {
        const QScreen *best = nullptr;
        int bestArea = -1;
        for (const QScreen *screen : QApplication::screens()) {
            QRect inter = screen->availableGeometry().intersected(rect);
            int area = inter.width() * inter.height();
            if (area > bestArea) {
                bestArea = area;
                best = screen;
            }
        }
        return best ? best : QApplication::primaryScreen();
    };

    QPoint savedPos = config.windowPosition();
    QPoint targetPos;
    if (!savedPos.isNull()) {
        // Restore saved position, clamped to the best-matching screen
        const QScreen *screen = bestScreenFor(QRect(savedPos, winSize));
        targetPos = clampedPos(savedPos, screen->availableGeometry());
    } else {
        // Default: bottom-right corner of primary screen
        const QRect screen = QApplication::primaryScreen()->availableGeometry();
        targetPos = QPoint(screen.right() - winSize.width() - posMargin,
                           screen.bottom() - winSize.height() - posMargin);
    }
    w.move(targetPos);

    // --- Load animation assets (legacy fallback) -----------------------------
    // If no sprite pack is loaded, fall back to hardcoded assets
    if (!packManager.activePack()) {
        QString spritePath, animJsonPath;
        if (!assetsDir.isEmpty()) {
            spritePath = assetsDir + "/map.png";
            animJsonPath = assetsDir + "/animations.json";
        }

        if (!spritePath.isEmpty()) {
            w.animationEngine()->loadAssets(spritePath, animJsonPath);
        }
    }

    // --- Tips engine ---------------------------------------------------------
    TipsEngine tipsEngine;
    tipsEngine.setAnimationEngine(w.animationEngine());
    tipsEngine.setTipBubble(w.tipBubbleWidget());

    // --- Event router --------------------------------------------------------
    EventRouter eventRouter;
    eventRouter.setAnimationEngine(w.animationEngine());
    eventRouter.setLottieEngine(w.lottieEngine());
#ifdef OAI_LIVE2D_SUPPORT
    eventRouter.setLive2dEngine(w.live2dEngine());
#endif
    eventRouter.setTipBubble(w.tipBubbleWidget());
    eventRouter.setTipsEngine(&tipsEngine);

    // Load event mappings from active sprite pack
    if (packManager.activePack()) {
        eventRouter.loadFromCharacterPack(packManager.activePack());
    }

    // Refresh event mappings when the active pack changes or is reloaded,
    // otherwise a previous pack's animation names (e.g. Clippy's PascalCase
    // "Greet"/"Think") leak into the new pack's engine (e.g. Live2D Hiyori).
    QObject::connect(&packManager, &CharacterPackManager::activePackChanged,
                     &eventRouter, &EventRouter::loadFromCharacterPack);
    QObject::connect(&packManager, &CharacterPackManager::packReloaded,
                     &eventRouter, &EventRouter::loadFromCharacterPack);

    // --- IPC server ----------------------------------------------------------
    IpcServer ipcServer;

    // IPC events → EventRouter
    QObject::connect(&ipcServer, &IpcServer::eventReceived,
                     &eventRouter, &EventRouter::routeEvent);

    // IPC tips → TipBubbleWidget directly
    QObject::connect(&ipcServer, &IpcServer::tipReceived,
                     &w, [&w](const QJsonObject &tip) {
        const QString title = tip.value("title").toString("Tip");
        const QString body = tip.value("body").toString();
        const QString anim = tip.value("animation").toString();

        w.tipBubbleWidget()->showBubble(title, body, TipBubbleWidget::TipBubble);

        if (anim.isEmpty()) return;

        // Route through whichever engine has animations loaded,
        // matching EventRouter's priority (Live2D > Lottie > Sprite).
#ifdef OAI_LIVE2D_SUPPORT
        if (w.live2dEngine() && w.live2dEngine()->hasAnimations()) {
            w.live2dEngine()->playAnimation(anim, Live2DAnimationEngine::NormalPriority);
            return;
        }
#endif
        if (w.lottieEngine() && w.lottieEngine()->hasAnimations()) {
            w.lottieEngine()->playAnimation(anim, LottieAnimationEngine::NormalPriority);
            return;
        }
        w.animationEngine()->playAnimation(anim, SpriteAnimationEngine::NormalPriority);
    });

    ipcServer.start(config.ipcEndpoint());

    // Restart IPC server when port changes
    QObject::connect(&config, &ConfigManager::ipcEndpointChanged,
                     &ipcServer, [&ipcServer](const QString &endpoint) {
        qDebug() << "IPC: Restarting server on new endpoint:" << endpoint;
        ipcServer.restart(endpoint);
    });

    // --- System tray ---------------------------------------------------------
    SystemTray tray(&w);
    tray.show();
    w.setSystemTray(&tray);

    // --- Update checker -------------------------------------------------------
    UpdateChecker updateChecker;
    tray.setUpdateChecker(&updateChecker);

    // Check for updates on startup (delayed to not block UI)
    QTimer::singleShot(5000, &updateChecker, &UpdateChecker::checkForUpdates);

    // --- Language switching --------------------------------------------------
    QObject::connect(&config, &ConfigManager::languageChanged,
                     &w, &MainWindow::onLanguageChanged);
    QObject::connect(&config, &ConfigManager::languageChanged,
                     &eventRouter, &EventRouter::retranslateUi);
    QObject::connect(&config, &ConfigManager::languageChanged,
                     &tipsEngine, &TipsEngine::retranslateUi);

    w.show();
    w.raise();
#ifdef Q_OS_MAC
    w.activateWindow();
#endif

    qDebug() << "Oai started — window at" << w.pos() << "size" << w.size();

    return a.exec();
}

#include "mainwindow.h"
#include "ConfigManager.h"
#include "IpcServer.h"
#include "EventRouter.h"
#include "SpriteAnimationEngine.h"
#include "LottieEffectOverlay.h"
#include "TipBubbleWidget.h"
#include "TipsEngine.h"
#include "SystemTray.h"

#include <QApplication>
#include <QLocale>
#include <QLockFile>
#include <QTranslator>
#include <QDir>
#include <QStandardPaths>
#include <QScreen>
#include <QDebug>

static QString configDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/Qlippy";
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Qlippy");
    a.setOrganizationName("Qlippy");
    a.setWindowIcon(QIcon(":/icons/clippy.png"));
    a.setQuitOnLastWindowClosed(false); // system tray keeps it alive

    // --- Single instance enforcement -----------------------------------------
    const QString lockDir = configDir();
    QDir().mkpath(lockDir);
    QLockFile lockFile(lockDir + "/Qlippy.lock");
    lockFile.setStaleLockTime(30000); // 30s stale timeout
    if (!lockFile.tryLock(100)) {
        qInfo() << "Qlippy is already running. Bringing existing instance to front.";
        return 0;
    }

    // --- Config --------------------------------------------------------------
    ConfigManager config;
    config.load();

    // --- Translations --------------------------------------------------------
    QTranslator translator;
    QString lang = config.language();
    if (!lang.isEmpty() && lang != "en") {
        const QString baseName = "Qlippy_" + lang;
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
        }
    } else {
        // Fall back to system locale
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = "Qlippy_" + QLocale(locale).name();
            if (translator.load(":/i18n/" + baseName)) {
                a.installTranslator(&translator);
                break;
            }
        }
    }

    // --- Main window ---------------------------------------------------------
    MainWindow w(&config, &translator);

    // Restore saved position (or default to center of primary screen)
    QPoint savedPos = config.windowPosition();
    if (!savedPos.isNull()) {
        w.move(savedPos);
    } else {
        const QRect screen = QApplication::primaryScreen()->availableGeometry();
        w.move(screen.right() - 220, screen.bottom() - 220);
    }

    // --- Load assets ---------------------------------------------------------
    // Character animations: sprite sheet from previous Electron-based version
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

    QString spritePath, animJsonPath, effectsDir;
    const QString assetsDir = findAssetsDir();

    if (!assetsDir.isEmpty()) {
        spritePath = assetsDir + "/map.png";
        animJsonPath = assetsDir + "/animations.json";
        effectsDir = assetsDir + "/lottie/effects";
        qDebug() << "Assets loaded from:" << assetsDir;
    } else {
        qWarning() << "Assets not found. Searched from:" << QApplication::applicationDirPath();
    }

    if (!spritePath.isEmpty()) {
        w.animationEngine()->loadAssets(spritePath, animJsonPath);
    }

    // Visual effects: Lottie JSON files
    if (!effectsDir.isEmpty() && QDir(effectsDir).exists()) {
        w.effectOverlay()->loadEffects(effectsDir);
    }

    // --- Tips engine ---------------------------------------------------------
    TipsEngine tipsEngine;
    tipsEngine.setAnimationEngine(w.animationEngine());
    tipsEngine.setTipBubble(w.tipBubbleWidget());

    // --- Event router --------------------------------------------------------
    EventRouter eventRouter;
    eventRouter.setAnimationEngine(w.animationEngine());
    eventRouter.setTipBubble(w.tipBubbleWidget());
    eventRouter.setEffectOverlay(w.effectOverlay());
    eventRouter.setTipsEngine(&tipsEngine);

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

        if (!anim.isEmpty()) {
            w.animationEngine()->playAnimation(anim, SpriteAnimationEngine::NormalPriority);
        }
    });

    ipcServer.start(config.ipcEndpoint());

    // --- System tray ---------------------------------------------------------
    SystemTray tray(&w);
    tray.show();
    w.setSystemTray(&tray);

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

    qDebug() << "Qlippy started — window at" << w.pos() << "size" << w.size();

    return a.exec();
}

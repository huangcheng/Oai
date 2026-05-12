#include "mainwindow.h"
#include "SpriteAnimationEngine.h"
#include "LottieAnimationEngine.h"
#ifdef OAI_LIVE2D_SUPPORT
#include "Live2DAnimationEngine.h"
#endif
#include "CharacterPackManager.h"
#include "CharacterPack.h"
#include "ConfigManager.h"
#include "TipBubbleWidget.h"
#include "SettingsPanelWidget.h"
#include "EcgWidget.h"
#include "SystemTray.h"
#include "EventRouter.h"
#include "TipsCatalog.h"
#include "FullscreenWatcher.h"
#include "PetStateMachine.h"

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include "StyledAlertDialog.h"
#include <QAction>
#include <QApplication>
#include <QRandomGenerator>
#include <QTranslator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QTimer>
#include <QShowEvent>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

#include "../thirdparty/miniz/miniz.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
// Older MinGW SDKs lack the Win11-era attribute IDs; fall back to the
// numeric values from the Microsoft docs.
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#endif

MainWindow::MainWindow(ConfigManager *config, QTranslator *translator, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
    , m_translator(translator)
{
    setupWindowFlags();

    // Enable drag-and-drop
    setAcceptDrops(true);

    // Receive mouseMoveEvent without a button held, so Live2D models can
    // track the cursor (head/eyes follow pointer).
    setMouseTracking(true);

    // Window is taller than the pet so the speech bubble fits above it
    setFixedSize(124, 200);

    // Initialize subsystems
    m_engine = new SpriteAnimationEngine(this);
    m_lottieEngine = new LottieAnimationEngine(this);
#ifdef OAI_LIVE2D_SUPPORT
    m_live2dEngine = new Live2DAnimationEngine(this);
#endif

    // Create floating widgets
    m_tipBubble = new TipBubbleWidget(nullptr); // no parent — separate top-level widget
    m_tipBubble->setAnchorRect(petRect());
    m_tipBubble->anchorTo(this);

    m_settingsPanel = new SettingsPanelWidget(m_config, nullptr);
    m_settingsPanel->setAnchorRect(petRect());
    m_settingsPanel->hide();

    m_ecgWidget = new EcgWidget(nullptr); // top-level, like the tip bubble
    m_ecgWidget->setAnchorRect(petRect());
    m_ecgWidget->anchorTo(this);

    // Wire ECG chassis drag so MainWindow tracks the same delta
    connect(m_ecgWidget, &EcgWidget::dragMoved, this, [this](QPoint delta) {
        move(pos() + delta);
        emit positionChanged(pos());
    });

    connect(m_ecgWidget, &EcgWidget::contextMenuRequested,
            this, &MainWindow::showContextMenu);

    connect(m_config, &ConfigManager::displayModeChanged,
            this, &MainWindow::onDisplayModeChanged);

    // Sync initial state once everything is constructed
    onDisplayModeChanged(m_config->displayMode());

    // Connect position change for config persistence
    connect(this, &MainWindow::positionChanged, m_config, &ConfigManager::setWindowPosition);

    // Reposition floating widgets when pet moves
    connect(this, &MainWindow::positionChanged, this, [this](const QPoint &) {
        m_tipBubble->setAnchorRect(petRect());
        m_tipBubble->anchorTo(this);
        if (m_settingsPanel->isVisible()) {
            m_settingsPanel->setAnchorRect(petRect());
            m_settingsPanel->anchorTo(this);
        }
        // Note: ECG widget intentionally NOT re-anchored here. In ECG mode
        // the widget owns its own position via chassis drag, and the
        // MainWindow position follows the ECG (not the other way around).
        // Re-anchoring would create a feedback loop that pins ECG in place.
    });

    // Connect effect triggers from animation engines
    // (Effects removed - using sprite animations only)

    // Repaint widget whenever animation engine advances a frame
    connect(m_engine, &SpriteAnimationEngine::frameChanged,
            this, QOverload<>::of(&QWidget::update));
    connect(m_lottieEngine, &LottieAnimationEngine::frameChanged,
            this, QOverload<>::of(&QWidget::update));
#ifdef OAI_LIVE2D_SUPPORT
    connect(m_live2dEngine, &Live2DAnimationEngine::frameChanged,
            this, QOverload<>::of(&QWidget::update));
#endif

#ifdef Q_OS_WIN
    // Refresh DWM attributes every 30s — display sleep/wake or DWM restart
    // can drop the corner-preference / backdrop / NC-rendering settings.
    // We also force a window-style refresh (SWP_FRAMECHANGED) so Windows
    // re-evaluates the composition surface, and queue a repaint so Qt
    // re-composites the widget.  The bubble and ECG widgets get the same
    // treatment so they don't disappear while the pet stays visible.
    m_dwmRefreshTimer = new QTimer(this);
    m_dwmRefreshTimer->setInterval(30000);
    connect(m_dwmRefreshTimer, &QTimer::timeout, this, [this]() {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        if (hwnd) {
            const int doNotRound = 1;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                  &doNotRound, sizeof(doNotRound));
            const int backdropNone = 1;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                  &backdropNone, sizeof(backdropNone));
            const int ncRenderingDisabled = 1;
            DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                                  &ncRenderingDisabled, sizeof(ncRenderingDisabled));

            // Re-apply the Qt attribute that suppresses the system background
            // paint — DWM restart can silently drop it, leaving a white rect.
            setAttribute(Qt::WA_NoSystemBackground, true);

            // Force Windows to re-evaluate the window frame / composition.
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_FRAMECHANGED | SWP_NOACTIVATE);

            // Ensure Qt schedules a repaint; without this the backing store
            // may stay stale after DWM recreates its composition surface.
            update();
        }

        // Keep the floating widgets in sync — they have their own native
        // windows and their DWM attributes can degrade independently.
        if (m_tipBubble) m_tipBubble->refreshDwmAttributes();
    });
    m_dwmRefreshTimer->start();
#endif

    // Gaming Mode: start fullscreen watcher if enabled at launch
    m_fullscreenWatcher = new FullscreenWatcher(this);
    connect(m_fullscreenWatcher, &FullscreenWatcher::fullscreenAppStarted,
            this, &MainWindow::onFullscreenStarted);
    connect(m_fullscreenWatcher, &FullscreenWatcher::fullscreenAppStopped,
            this, &MainWindow::onFullscreenStopped);
    if (m_config->gamingModeEnabled())
        m_fullscreenWatcher->start();

    connect(m_config, &ConfigManager::gamingModeEnabledChanged,
            this, [this](bool enabled) {
        if (enabled) {
            m_fullscreenWatcher->start();
        } else {
            m_fullscreenWatcher->stop();
            // Restore windows if they were hidden by Gaming Mode
            if (m_hiddenByGamingMode) {
                m_hiddenByGamingMode = false;
                if (m_visible)
                    onDisplayModeChanged(m_config->displayMode());
            }
        }
    });
}

MainWindow::~MainWindow()
{
    // Tear down the three top-level widgets (constructed with nullptr parent
    // so they're separate windows, but tracked here as raw pointer members).
    // Stop the ECG widget first to halt its timers + audio, otherwise a tick
    // fired between hide() and delete can touch a widget that's mid-destruct.
    if (m_ecgWidget) {
        m_ecgWidget->stop();
        delete m_ecgWidget;
        m_ecgWidget = nullptr;
    }
    if (m_settingsPanel) {
        delete m_settingsPanel;
        m_settingsPanel = nullptr;
    }
    if (m_tipBubble) {
        delete m_tipBubble;
        m_tipBubble = nullptr;
    }
}

void MainWindow::setupWindowFlags()
{
    setWindowFlags(
        Qt::FramelessWindowHint        // No window frame
        | Qt::WindowStaysOnTopHint     // Always on top
        | Qt::Tool                     // No taskbar entry
        | Qt::WindowDoesNotAcceptFocus // Don't steal focus
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    // Windows DWM otherwise paints a default white background BEFORE
    // paintEvent runs (TranslucentBackground alone isn't enough on Win32),
    // and once it sees an opaque rectangle it adds a drop shadow + light
    // edge — visually a frame around the pet. Suppressing the system
    // background makes the window genuinely transparent on every platform.
    setAttribute(Qt::WA_NoSystemBackground, true);
#ifdef Q_OS_MAC
    // macOS: tool windows are hidden when the app is not active.
    // This keeps the pet visible at all times.
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    // Windows 11's DWM auto-applies rounded corners, a drop shadow, and a
    // Mica/Acrylic backdrop tint to top-level windows by default — even
    // frameless tool windows with TranslucentBackground+NoSystemBackground
    // get the chrome. Opt out per-window via the DWM API. winId() is valid
    // by showEvent() because the native window has just been realised.
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        const int doNotRound = 1;          // DWMWCP_DONOTROUND
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &doNotRound, sizeof(doNotRound));
        const int backdropNone = 1;        // DWMSBT_NONE (Win11 22H2+)
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                              &backdropNone, sizeof(backdropNone));
        const int ncRenderingDisabled = 1; // DWMNCRP_DISABLED
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                              &ncRenderingDisabled, sizeof(ncRenderingDisabled));
    }
#endif
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_DISPLAYCHANGE) {
            // Display resolution/depth changed (display sleep/wake, monitor
            // connect/disconnect, RDP session).  DWM may have restarted, so
            // re-apply attributes immediately instead of waiting for the
            // 30-second timer.
            HWND hwnd = reinterpret_cast<HWND>(winId());
            if (hwnd) {
                const int doNotRound = 1;
                DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                      &doNotRound, sizeof(doNotRound));
                const int backdropNone = 1;
                DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                      &backdropNone, sizeof(backdropNone));
                const int ncRenderingDisabled = 1;
                DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                                      &ncRenderingDisabled, sizeof(ncRenderingDisabled));

                setAttribute(Qt::WA_NoSystemBackground, true);
                SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                             SWP_FRAMECHANGED | SWP_NOACTIVATE);
                update();
            }
            if (m_tipBubble) m_tipBubble->refreshDwmAttributes();
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect pet = petRect();

    // Draw character animation (Live2D, Lottie, or sprite sheet).
    // On Windows, Live2D's OpenGL context can be invalidated by DWM restart
    // or GPU power-state changes. Fall back through engines if the current
    // one fails to produce a frame.
#ifdef OAI_LIVE2D_SUPPORT
    if (m_live2dEngine && m_live2dEngine->isPlaying()) {
        if (m_live2dEngine->lastPaintSuccessful()) {
            m_live2dEngine->paint(&painter, pet);
        } else if (m_lottieEngine && m_lottieEngine->isPlaying()) {
            m_lottieEngine->paint(&painter, pet);
        } else if (m_engine) {
            m_engine->paint(&painter, pet);
        }
    } else
#endif
    if (m_lottieEngine && m_lottieEngine->isPlaying()) {
        m_lottieEngine->paint(&painter, pet);
    } else if (m_engine) {
        m_engine->paint(&painter, pet);
    }

    // (Speech bubbles are now shown via TipBubbleWidget)
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isInPetRect(event->pos())) {
        m_dragStartPos = event->globalPosition().toPoint();
        m_dragWindowPos = pos();
        m_dragging = false;
    }
    QWidget::mousePressEvent(event);
}

QRect MainWindow::petRect() const
{
    int y = height() - m_petSize.height();
    if (y < 0) y = 0;
    return QRect(0, y, m_petSize.width(), m_petSize.height());
}

bool MainWindow::isInPetRect(const QPoint &pos) const
{
    return petRect().contains(pos);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // Feed the Live2D drag manager so the head/eyes track the cursor.
    // Map pointer position inside petRect to normalized (-1..+1), Y-up
    // (Cubism convention). Skip for clicks that are starting a window drag.
#ifdef OAI_LIVE2D_SUPPORT
    if (m_live2dEngine && isInPetRect(event->pos())) {
        const QRect pet = petRect();
        const float nx = 2.0f * (event->pos().x() - pet.x()) / float(pet.width())  - 1.0f;
        const float ny = 1.0f - 2.0f * (event->pos().y() - pet.y()) / float(pet.height());
        m_live2dEngine->setPointerTarget(nx, ny);
    }
#endif

    if (event->buttons() & Qt::LeftButton) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;

        if (!m_dragging && delta.manhattanLength() > DRAG_THRESHOLD) {
            m_dragging = true;
            // Only sprite packs ship a 'gesture_down' animation. For Live2D
            // the drag reaction is already provided by pointer tracking.
            if (m_engine->hasAnimations()) {
                m_engine->playAnimation("gesture_down", SpriteAnimationEngine::HighPriority);
            }
        }

        if (m_dragging) {
            move(m_dragWindowPos + delta);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void MainWindow::leaveEvent(QEvent *event)
{
#ifdef OAI_LIVE2D_SUPPORT
    if (m_live2dEngine) m_live2dEngine->setPointerTarget(0.0f, 0.0f);
#endif
    if (m_config && m_config->mouseTrackingEnabled()) {
        if (m_stateMachine) {
            m_stateMachine->onSyntheticEvent(QStringLiteral("user.hoverLeave"));
        }
    }
    QWidget::leaveEvent(event);
}

void MainWindow::enterEvent(QEnterEvent *event)
{
#ifdef OAI_LIVE2D_SUPPORT
    if (m_live2dEngine) {
        const QRect pr = petRect();
        const QPointF center = pr.center();
        m_live2dEngine->setPointerTarget(
            static_cast<float>(center.x()),
            static_cast<float>(center.y()));
    }
#endif
    if (m_config && m_config->mouseTrackingEnabled()) {
        if (m_stateMachine) {
            m_stateMachine->onSyntheticEvent(QStringLiteral("user.hoverEnter"));
        }
    }
    QWidget::enterEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_dragging) {
            m_dragging = false;
            if (m_engine->hasAnimations()) {
                m_engine->playAnimation("lookdown", SpriteAnimationEngine::HighPriority);
                m_engine->playAnimation("rest", SpriteAnimationEngine::NormalPriority);
            }
            emit positionChanged(pos());
        } else if (isInPetRect(event->pos())) {
            // Route mouse-click through FSM so the state machine handles
            // user interaction and can trigger the appropriate animation chain.
            if (m_stateMachine) {
                m_stateMachine->onSyntheticEvent(QStringLiteral("user.click"));
                showRandomGreeting();
                QWidget::mouseReleaseEvent(event);
                return;
            }
            // Fallback for sprite packs without an event router wired.
            const QStringList clickAnims = {"click1", "click2"};
            const QString anim = clickAnims.at(QRandomGenerator::global()->bounded(clickAnims.size()));
            m_engine->playAnimation(anim, SpriteAnimationEngine::HighPriority);
            showRandomGreeting();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isInPetRect(event->pos())) {
        if (m_stateMachine) {
            m_stateMachine->onSyntheticEvent(QStringLiteral("user.doubleclick"));
            showRandomGreeting();
            QWidget::mouseDoubleClickEvent(event);
            return;
        }
        const QStringList dblAnims = {"doubleclick1", "doubleclick2"};
        const QString anim = dblAnims.at(QRandomGenerator::global()->bounded(dblAnims.size()));
        m_engine->playAnimation(anim, SpriteAnimationEngine::HighPriority);
        showRandomGreeting();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    showContextMenu(event->globalPos());
}

void MainWindow::showContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    int menuFontSize = 10;
#ifdef Q_OS_MAC
    menuFontSize = 13;
#endif
    QFont menuFont("HarmonyOS Sans SC", menuFontSize);
    menuFont.setStyleStrategy(QFont::PreferAntialias);
    menu.setFont(menuFont);

    QAction *toggleAction = menu.addAction(m_visible ? tr("Hide") : tr("Show"));
    connect(toggleAction, &QAction::triggered, this, &MainWindow::toggleVisibility);

    menu.addSeparator();

    QAction *settingsAction = menu.addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    QAction *aboutAction = menu.addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        const auto t = TipsCatalog::instance().message(QStringLiteral("about"));
        // In Character mode the tip bubble is the natural surface; in ECG
        // mode it's suppressed, so showBubble() would silently no-op. Fall
        // back to a modal dialog there so the About action works in both.
        if (m_tipBubble && !m_tipBubble->isSuppressed()) {
            m_tipBubble->showBubble(t.title, t.body, TipBubbleWidget::TipBubble);
        } else {
            StyledAlertDialog *dialog = new StyledAlertDialog(nullptr);
            dialog->setPetWindow(this);
            connect(dialog, &StyledAlertDialog::dismissed, dialog, &QObject::deleteLater);
            dialog->showAlert(t.title, t.body);
        }
    });

    menu.addSeparator();

    QAction *quitAction = menu.addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    menu.exec(globalPos);
}

bool MainWindow::isValidCodexPet(const QString &filePath) const
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, filePath.toUtf8().constData(), 0)) {
        qWarning() << "MainWindow: isValidCodexPet — cannot open ZIP:" << filePath;
        return false;
    }

    int petJsonIdx = mz_zip_reader_locate_file(&zip, "pet.json", nullptr, 0);
    if (petJsonIdx < 0) {
        qWarning() << "MainWindow: isValidCodexPet — no pet.json in:" << filePath;
        mz_zip_reader_end(&zip);
        return false;
    }

    size_t petJsonSize = 0;
    void *petJsonData = mz_zip_reader_extract_to_heap(&zip, petJsonIdx, &petJsonSize, 0);
    mz_zip_reader_end(&zip);

    if (!petJsonData) {
        qWarning() << "MainWindow: isValidCodexPet — failed to extract pet.json from:" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray(static_cast<const char *>(petJsonData), static_cast<int>(petJsonSize)));
    mz_free(petJsonData);

    if (!doc.isObject()) {
        qWarning() << "MainWindow: isValidCodexPet — invalid JSON in pet.json:" << filePath;
        return false;
    }

    const QJsonObject obj = doc.object();
    QString id = obj.value("id").toString();
    QString displayName = obj.value("displayName").toString();

    bool valid = !id.isEmpty() && !displayName.isEmpty();
    if (!valid) {
        qWarning() << "MainWindow: isValidCodexPet — missing id or displayName in:" << filePath;
    }
    return valid;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    qDebug() << "MainWindow: dragEnterEvent";
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            QString path = url.toLocalFile();
            qDebug() << "MainWindow: checking drag path:" << path;
            if (path.endsWith(".opk", Qt::CaseInsensitive)) {
                qDebug() << "MainWindow: accepting .opk drag";
                event->acceptProposedAction();
                return;
            }
            if (path.endsWith(".codex-pet", Qt::CaseInsensitive) ||
                path.endsWith(".codex-pet.zip", Qt::CaseInsensitive)) {
                bool valid = isValidCodexPet(path);
                qDebug() << "MainWindow: .codex-pet valid =" << valid;
                if (valid) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
    qDebug() << "MainWindow: dragEnterEvent ignored";
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!m_packManager) {
        qWarning() << "MainWindow: dropEvent ignored — no pack manager";
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    qDebug() << "MainWindow: dropEvent with" << urls.size() << "URLs";

    for (const QUrl &url : urls) {
        const QString filePath = url.toLocalFile();
        qDebug() << "MainWindow: dropped file:" << filePath;

        if (filePath.endsWith(".opk", Qt::CaseInsensitive)) {
            const bool installed = m_packManager->installPack(filePath);
            const QString msgId = installed
                                      ? QStringLiteral("pack.installed")
                                      : QStringLiteral("pack.install_failed");
            const auto t = TipsCatalog::instance().message(msgId);
            m_tipBubble->showBubble(t.title, t.body, TipBubbleWidget::TipBubble);
            qDebug() << "MainWindow: .opk install" << (installed ? "succeeded" : "failed");
        } else if ((filePath.endsWith(".codex-pet", Qt::CaseInsensitive) ||
                    filePath.endsWith(".codex-pet.zip", Qt::CaseInsensitive)) &&
                   isValidCodexPet(filePath)) {
            QFileInfo fi(filePath);
            QString userPacksDir = m_packManager->userDir();
            if (userPacksDir.isEmpty()) {
                userPacksDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/Oai/packs";
                qWarning() << "MainWindow: userDir empty, falling back to" << userPacksDir;
            }
            QDir().mkpath(userPacksDir);
            QString destFile = userPacksDir + "/" + fi.fileName();

            bool ok = false;
            if (QFile::exists(destFile)) {
                QFile::remove(destFile);
            }
            ok = QFile::copy(filePath, destFile);
            qDebug() << "MainWindow: copied .codex-pet to" << destFile << "=" << ok;

            if (ok) {
                QString builtInDir = m_packManager->builtInDir();
                QString currentPackId = m_packManager->activePackId();
                m_packManager->initialize(builtInDir, userPacksDir, currentPackId);
                const auto t = TipsCatalog::instance().message(QStringLiteral("pack.installed"));
                m_tipBubble->showBubble(t.title, t.body, TipBubbleWidget::TipBubble);
            } else {
                const auto t = TipsCatalog::instance().message(QStringLiteral("pack.install_failed"));
                m_tipBubble->showBubble(t.title, t.body, TipBubbleWidget::TipBubble);
            }
        } else {
            qDebug() << "MainWindow: dropped file ignored (not .opk or valid .codex-pet):" << filePath;
        }
    }

    event->acceptProposedAction();
}

void MainWindow::onDisplayModeChanged(ConfigManager::DisplayMode mode)
{
    if (mode == ConfigManager::DisplayMode::Ecg) {
        m_tipBubble->hideBubble();
        m_tipBubble->setSuppressed(true);
        hide();
        if (m_ecgWidget) {
            m_ecgWidget->setAnchorRect(petRect());
            m_ecgWidget->anchorTo(this);
            m_ecgWidget->start();
        }
    } else {
        if (m_ecgWidget) m_ecgWidget->stop();
        m_tipBubble->setSuppressed(false);
        show();
    }
}

void MainWindow::onFullscreenStarted()
{
    // Only hide if the pet is currently visible and not already hidden by us
    if (!m_hiddenByGamingMode && m_visible) {
        m_hiddenByGamingMode = true;
        hide();
        if (m_tipBubble) {
            m_tipBubble->hideBubble();
            m_tipBubble->setSuppressed(true);
        }
        if (m_ecgWidget && m_ecgWidget->isVisible())
            m_ecgWidget->hide();
        qDebug() << "MainWindow: Gaming Mode — hiding pet (fullscreen app detected)";
    }
}

void MainWindow::onFullscreenStopped()
{
    if (m_hiddenByGamingMode) {
        m_hiddenByGamingMode = false;
        if (m_visible) {
            onDisplayModeChanged(m_config->displayMode());
            if (m_ecgWidget && !m_ecgWidget->isVisible()
                    && m_config->displayMode() == ConfigManager::DisplayMode::Ecg)
                m_ecgWidget->show();
        }
        qDebug() << "MainWindow: Gaming Mode — restoring pet (fullscreen app gone)";
    }
}

void MainWindow::toggleVisibility()
{
    m_visible = !m_visible;
    if (m_visible) {
        // Restore to current mode
        onDisplayModeChanged(m_config->displayMode());
        if (m_config->displayMode() == ConfigManager::DisplayMode::Character) {
            m_engine->playAnimation("wave", SpriteAnimationEngine::HighPriority);
        }
    } else {
        hide();
        m_tipBubble->hideBubble();
        m_tipBubble->setSuppressed(true);
        m_settingsPanel->hideAnimated();
        if (m_ecgWidget) m_ecgWidget->stop();
    }
}

void MainWindow::openSettings()
{
    // In ECG mode this MainWindow is hidden, so anchoring Settings to its
    // petRect() lands the panel wherever MainWindow's last known coordinates
    // were — which is wrong if the user has dragged the ECG away. Anchor to
    // the ECG widget itself when it's the active display.
    if (m_ecgWidget && m_ecgWidget->isVisible()) {
        m_settingsPanel->setAnchorRect(QRect(0, 0, m_ecgWidget->width(), m_ecgWidget->height()));
        m_settingsPanel->anchorTo(m_ecgWidget);
    } else {
        m_settingsPanel->setAnchorRect(petRect());
        m_settingsPanel->anchorTo(this);
    }
    m_settingsPanel->showAnimated();
}

void MainWindow::setSystemTray(SystemTray *tray)
{
    m_systemTray = tray;
    if (m_systemTray && m_packManager) {
        m_systemTray->setCharacterPackManager(m_packManager);
    }
}

void MainWindow::setCharacterPackManager(CharacterPackManager *manager)
{
    m_packManager = manager;
    if (m_packManager) {
        m_packManager->setActiveLocale(m_config ? m_config->language() : QString());

        connect(m_packManager, &CharacterPackManager::activePackChanged,
                this, &MainWindow::onActivePackChanged);

        // Pass to settings panel
        if (m_settingsPanel) {
            m_settingsPanel->setCharacterPackManager(manager);
        }

        // Load active pack immediately (pack may have been loaded before signal was connected)
        if (m_packManager->activePack()) {
            onActivePackChanged();
        }
    }
}

void MainWindow::onActivePackChanged()
{
    qDebug() << "[ONACTIVE] onActivePackChanged called";
    if (!m_packManager) {
        qDebug() << "  no pack manager";
        return;
    }

    CharacterPack *pack = m_packManager->activePack();
    if (!pack) {
        qDebug() << "  no active pack";
        return;
    }

    qDebug() << "  Active pack id:" << m_packManager->activePackId()
             << "name:" << pack->metadata().name
             << "engineType:" << static_cast<int>(pack->characterConfig().engineType)
             << "isValid:" << pack->isValid();

    // Resize window based on pack frame dimensions × the pack's displayScale.
    // Default displayScale is 1.0 (native resolution). Sprite packs can opt
    // into a larger rendered size per-manifest to match Live2D's 300×300,
    // trading sharpness for visual parity — it's a per-pack call, not a
    // global normalization (which blurs low-res ClippyJS art unconditionally).
    int fw = pack->characterConfig().frameWidth;
    int fh = pack->characterConfig().frameHeight;
    const float displayScale = pack->characterConfig().displayScale;
    if (fw > 0 && fh > 0) {
        const int displayW = static_cast<int>(fw * displayScale);
        const int displayH = static_cast<int>(fh * displayScale);
        int tipSpace = height() - petRect().height();
        // Resize window first so height() updates before m_petSize changes,
        // avoiding a transient state where petRect() computes a negative y.
        setFixedSize(displayW, displayH + tipSpace);
        m_petSize = QSize(displayW, displayH);
        qDebug() << "  Window resized to:" << displayW << "x" << displayH;
    }

    if (m_ecgWidget && m_ecgWidget->isVisible()) {
        m_ecgWidget->setAnchorRect(petRect());
        m_ecgWidget->anchorTo(this);
    }

    // Load animations based on pack type.
    // Stop every engine first: all three share the paint path, and Live2D
    // takes priority in paintEvent if isPlaying() is true — without this,
    // switching from a Live2D pack to a sprite pack would leave the old
    // Live2D frame squished into the sprite pack's smaller petRect.
    m_engine->stop();
    m_lottieEngine->stop();
#ifdef OAI_LIVE2D_SUPPORT
    m_live2dEngine->stop();
#endif

#ifndef OAI_LIVE2D_SUPPORT
    // If Live2D support was not compiled in but the selected pack requires
    // it, warn and auto-skip to the first non-Live2D pack.  Without this
    // fallback the sprite engine fails silently and the pet window renders
    // as a fully transparent rectangle.
    if (pack->characterConfig().engineType == CharacterPack::EngineType::Live2D) {
        qWarning() << "Live2D pack" << pack->metadata().name
                    << "selected but Live2D support not compiled in."
                    << "See CONTRIBUTING.md for Cubism SDK Core setup.";
        // Guard against recursion — switchPack re-emits activePackChanged.
        if (m_skipLive2dFallback) return;
        m_skipLive2dFallback = true;
        if (m_packManager) {
            const auto packs = m_packManager->availablePacks();
            for (const auto &info : packs) {
                if (info.id == pack->metadata().id) continue; // skip the Live2D pack
                m_packManager->switchPack(info.id);
                // onActivePackChanged recurses here; if the new pack loads,
                // one of the engines will be playing and we're done.
                if (m_lottieEngine->isPlaying() || m_engine->isPlaying()) {
                    m_skipLive2dFallback = false;
                    return;
                }
            }
        }
        m_skipLive2dFallback = false;
        qWarning() << "No non-Live2D packs available — pet will be invisible";
        return;
    }
#endif

#ifdef OAI_LIVE2D_SUPPORT
    if (pack->characterConfig().engineType == CharacterPack::EngineType::Live2D) {
        m_live2dEngine->loadFromCharacterPack(pack);
    } else
#endif
    if (pack->characterConfig().engineType == CharacterPack::EngineType::Lottie) {
        m_lottieEngine->loadFromCharacterPack(pack);
    } else {
        m_engine->loadFromCharacterPack(pack);
    }

#ifdef OAI_LIVE2D_SUPPORT
    // Crop the window to the character's actual silhouette once the Live2D
    // engine has produced a few frames (motion settles ~500ms after load).
    // Without this, Rice / Wanko / etc. render as a small character at the
    // bottom of a 300×300 frame with a huge empty top margin, and the tip
    // bubble anchors above that empty space instead of above the character.
    if (pack->characterConfig().engineType == CharacterPack::EngineType::Live2D) {
        const float displayScale = pack->characterConfig().displayScale;
        // Stamp this load attempt so a rapid re-trigger invalidates the
        // pending crop callback below; only the most recent load applies.
        const int loadId = ++m_packLoadId;
        QTimer::singleShot(500, this, [this, displayScale, loadId]() {
            if (loadId != m_packLoadId) return;        // superseded
            if (!m_live2dEngine) return;
            const QRect b = m_live2dEngine->characterBounds();
            if (b.isNull() || b.isEmpty()) return;
            // Match Live2DAnimationEngine::paint()'s source rect: full frame
            // width + measured height + generous top pad for motion headroom
            // and lighting/glow effects above the character.
            const int padTop = std::max(32, b.height() / 4);
            const int srcH = std::min(m_live2dEngine->renderHeight() - std::max(0, b.y() - padTop),
                                      b.height() + padTop);
            const int displayW = static_cast<int>(m_live2dEngine->renderWidth() * displayScale);
            const int displayH = static_cast<int>(srcH * displayScale);
            const int tipSpace = height() - petRect().height();
            setFixedSize(displayW, displayH + tipSpace);
            m_petSize = QSize(displayW, displayH);
            // Re-anchor floating widgets to the newly sized pet rect.
            m_tipBubble->setAnchorRect(petRect());
            m_tipBubble->anchorTo(this);
            if (m_ecgWidget && m_ecgWidget->isVisible()) {
                m_ecgWidget->setAnchorRect(petRect());
                m_ecgWidget->anchorTo(this);
            }
            update();
        });
    }
#endif
}

void MainWindow::retranslateUi()
{
    // Context menu is ephemeral — it will pick up translations on next show.
    // Nothing persistent to update here except the About bubble text which is also ephemeral.
}

void MainWindow::reloadTranslator(const QString &lang)
{
    QApplication *app = qApp;
    app->removeTranslator(m_translator);

    if (!lang.isEmpty() && lang != "en") {
        const QString baseName = "Oai_" + lang;
        if (m_translator->load(":/i18n/" + baseName)) {
            app->installTranslator(m_translator);
        }
    }
}

void MainWindow::onLanguageChanged(const QString &lang)
{
    reloadTranslator(lang);
    TipsCatalog::instance().setLocale(lang);
    if (m_packManager) {
        m_packManager->setActiveLocale(lang);
    }
    retranslateUi();
    m_settingsPanel->retranslateUi();
    if (m_systemTray) {
        m_systemTray->retranslateUi();
    }
}

void MainWindow::showRandomGreeting()
{
    if (!m_tipBubble) return;
    const auto g = TipsCatalog::instance().randomGreeting();
    if (!g.title.isEmpty()) {
        m_tipBubble->showBubble(g.title, g.body, TipBubbleWidget::TipBubble);
    }
}

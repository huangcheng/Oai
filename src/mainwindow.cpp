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

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QRandomGenerator>
#include <QTranslator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QTimer>
#include <QShowEvent>
#include <algorithm>

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
    if (m_config->ecgEnabled()) {
        m_ecgWidget->start();
    }

    connect(m_config, &ConfigManager::ecgEnabledChanged,
            this, [this](bool enabled) {
        if (enabled) {
            m_ecgWidget->setAnchorRect(petRect());
            m_ecgWidget->anchorTo(this);
            m_ecgWidget->start();
        } else {
            m_ecgWidget->stop();
        }
    });

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
        if (m_ecgWidget && m_ecgWidget->isVisible()) {
            m_ecgWidget->setAnchorRect(petRect());
            m_ecgWidget->anchorTo(this);
        }
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
}

MainWindow::~MainWindow()
{
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

void MainWindow::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect pet = petRect();

    // Draw character animation (Live2D, Lottie, or sprite sheet)
#ifdef OAI_LIVE2D_SUPPORT
    if (m_live2dEngine && m_live2dEngine->isPlaying()) {
        m_live2dEngine->paint(&painter, pet);
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
    return QRect(0, height() - m_petSize.height(), m_petSize.width(), m_petSize.height());
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
    QWidget::leaveEvent(event);
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
            // Route mouse-click through EventRouter so the manifest's
            // eventMap declares the fallback chain — engine has no
            // hardcoded knowledge of group names like "TouchBody"/"Tap".
            if (m_eventRouter) {
                m_eventRouter->triggerEvent(QStringLiteral("user.click"));
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
        if (m_eventRouter) {
            m_eventRouter->triggerEvent(QStringLiteral("user.doubleclick"));
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
    QMenu menu(this);
    // Platform-specific font size: macOS uses 13, Windows uses 10
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
        m_tipBubble->showBubble(t.title, t.body, TipBubbleWidget::TipBubble);
    });

    menu.addSeparator();

    QAction *quitAction = menu.addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    menu.exec(event->globalPos());
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept drag if it contains .opk files
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.toLocalFile().endsWith(".opk", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!m_packManager) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        const QString filePath = url.toLocalFile();
        if (filePath.endsWith(".opk", Qt::CaseInsensitive)) {
            // Install the pack
            const QString msgId = m_packManager->installPack(filePath)
                                      ? QStringLiteral("pack.installed")
                                      : QStringLiteral("pack.install_failed");
            const auto t = TipsCatalog::instance().message(msgId);
            m_tipBubble->showBubble(t.title, t.body, TipBubbleWidget::TipBubble);
        }
    }

    event->acceptProposedAction();
}

void MainWindow::toggleVisibility()
{
    m_visible = !m_visible;
    if (m_visible) {
        show();
        m_engine->playAnimation("wave", SpriteAnimationEngine::HighPriority);
        if (m_ecgWidget && m_config->ecgEnabled()) {
            m_ecgWidget->setAnchorRect(petRect());
            m_ecgWidget->anchorTo(this);
            m_ecgWidget->start();
        }
    } else {
        hide();
        m_tipBubble->hideBubble();
        m_settingsPanel->hideAnimated();
        if (m_ecgWidget) m_ecgWidget->stop();
    }
}

void MainWindow::openSettings()
{
    m_settingsPanel->setAnchorRect(petRect());
    m_settingsPanel->anchorTo(this);
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
    if (!m_packManager) {
        return;
    }

    CharacterPack *pack = m_packManager->activePack();
    if (!pack) {
        return;
    }

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
        m_petSize = QSize(displayW, displayH);
        setFixedSize(displayW, displayH + tipSpace);
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
        QTimer::singleShot(500, this, [this, displayScale]() {
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
            m_petSize = QSize(displayW, displayH);
            setFixedSize(displayW, displayH + tipSpace);
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

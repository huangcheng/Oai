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
#include "SystemTray.h"

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
#include <algorithm>

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
#ifdef Q_OS_MAC
    // macOS: tool windows are hidden when the app is not active.
    // This keeps the pet visible at all times.
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
    // Ensure the frameless translucent window composites correctly.
    setAttribute(Qt::WA_NoSystemBackground, true);
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
            m_engine->playAnimation("gesture_down", SpriteAnimationEngine::HighPriority);
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
            m_engine->playAnimation("lookdown", SpriteAnimationEngine::HighPriority);
            m_engine->playAnimation("rest", SpriteAnimationEngine::NormalPriority);
            emit positionChanged(pos());
        } else if (isInPetRect(event->pos())) {
#ifdef OAI_LIVE2D_SUPPORT
            if (m_live2dEngine && m_live2dEngine->hasAnimations()) {
                m_live2dEngine->tap();
                showRandomGreeting();
                QWidget::mouseReleaseEvent(event);
                return;
            }
#endif
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
#ifdef OAI_LIVE2D_SUPPORT
        if (m_live2dEngine && m_live2dEngine->hasAnimations()) {
            m_live2dEngine->tap();
            showRandomGreeting();
            QWidget::mouseDoubleClickEvent(event);
            return;
        }
#endif
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
        m_tipBubble->showBubble(tr("About"), tr("Oai Desktop Pet\nv1.2.0"), TipBubbleWidget::TipBubble);
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
            if (m_packManager->installPack(filePath)) {
                m_tipBubble->showBubble(
                    tr("Pack Installed"),
                    tr("Sprite pack installed successfully!"),
                    TipBubbleWidget::TipBubble
                );
            } else {
                m_tipBubble->showBubble(
                    tr("Installation Failed"),
                    tr("Failed to install sprite pack."),
                    TipBubbleWidget::TipBubble
                );
            }
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
    } else {
        hide();
        m_tipBubble->hideBubble();
        m_settingsPanel->hide();
    }
}

void MainWindow::openSettings()
{
    m_settingsPanel->setAnchorRect(petRect());
    m_settingsPanel->show();
    m_settingsPanel->anchorTo(this);
    m_settingsPanel->raise();
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
            const int displayW = static_cast<int>(b.width() * displayScale);
            const int displayH = static_cast<int>(b.height() * displayScale);
            const int tipSpace = height() - petRect().height();
            m_petSize = QSize(displayW, displayH);
            setFixedSize(displayW, displayH + tipSpace);
            // Re-anchor floating widgets to the newly sized pet rect.
            m_tipBubble->setAnchorRect(petRect());
            m_tipBubble->anchorTo(this);
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
    retranslateUi();
    m_settingsPanel->retranslateUi();
    if (m_systemTray) {
        m_systemTray->retranslateUi();
    }
}

void MainWindow::showRandomGreeting()
{
    if (!m_tipBubble) return;

    struct Greeting {
        const char *title;
        const char *message;
    };
    static const Greeting greetings[] = {
        {QT_TR_NOOP("Hi there!"), QT_TR_NOOP("Need any help today?")},
        {QT_TR_NOOP("Hey!"), QT_TR_NOOP("I'm watching you code. Don't mess up!")},
        {QT_TR_NOOP("Hello!"), QT_TR_NOOP("Ready to build something amazing?")},
        {QT_TR_NOOP("Psst..."), QT_TR_NOOP("Remember to save your work often!")},
        {QT_TR_NOOP("Yo!"), QT_TR_NOOP("That code looks pretty good. Keep it up!")},
        {QT_TR_NOOP("Hi!"), QT_TR_NOOP("Let me know if you need a second pair of eyes.")},
        {QT_TR_NOOP("Hey there!"), QT_TR_NOOP("Don't forget to take breaks and stretch!")},
        {QT_TR_NOOP("Hiya!"), QT_TR_NOOP("Coffee break? I'm just a click away.")},
        {QT_TR_NOOP("Oh hello!"), QT_TR_NOOP("I see you're coding. Want a tip?")},
        {QT_TR_NOOP("Greetings!"), QT_TR_NOOP("The code is strong with this one.")},
    };

    const int idx = QRandomGenerator::global()->bounded(static_cast<int>(sizeof(greetings) / sizeof(greetings[0])));
    m_tipBubble->showBubble(tr(greetings[idx].title), tr(greetings[idx].message), TipBubbleWidget::TipBubble);
}

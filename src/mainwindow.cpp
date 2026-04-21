#include "mainwindow.h"
#include "SpriteAnimationEngine.h"
#include "LottieEffectOverlay.h"
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

MainWindow::MainWindow(ConfigManager *config, QTranslator *translator, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
    , m_translator(translator)
{
    setupWindowFlags();

    // Window is taller than the pet so the speech bubble fits above it
    setFixedSize(124, 200);

    // Initialize subsystems
    m_engine = new SpriteAnimationEngine(this);
    m_effects = new LottieEffectOverlay(this);

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

    // Connect effect triggers from animation engine
    connect(m_engine, &SpriteAnimationEngine::effectRequested,
            m_effects, &LottieEffectOverlay::triggerEffect);

    // Repaint widget whenever animation engine advances a frame
    connect(m_engine, &SpriteAnimationEngine::frameChanged,
            this, QOverload<>::of(&QWidget::update));
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

    // Draw character animation (sprite sheet)
    if (m_engine) {
        m_engine->paint(&painter, pet);
    }

    // Draw visual effects on top (Lottie)
    if (m_effects) {
        m_effects->paint(&painter, pet);
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
    return QRect(0, height() - 93, 124, 93);
}

bool MainWindow::isInPetRect(const QPoint &pos) const
{
    return petRect().contains(pos);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
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

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_dragging) {
            m_dragging = false;
            m_engine->playAnimation("lookdown", SpriteAnimationEngine::HighPriority);
            m_engine->playAnimation("rest", SpriteAnimationEngine::NormalPriority);
            emit positionChanged(pos());
        } else if (isInPetRect(event->pos())) {
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

    QAction *toggleAction = menu.addAction(m_visible ? tr("Hide") : tr("Show"));
    connect(toggleAction, &QAction::triggered, this, &MainWindow::toggleVisibility);

    menu.addSeparator();

    QAction *aboutAction = menu.addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        m_tipBubble->showBubble(tr("About"), tr("Qlippy Desktop Pet\nv1.0.0"), TipBubbleWidget::TipBubble);
    });

    menu.addSeparator();

    QAction *settingsAction = menu.addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    menu.addSeparator();

    QAction *quitAction = menu.addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    menu.exec(event->globalPos());
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
    m_settingsPanel->anchorTo(this);
    m_settingsPanel->show();
    m_settingsPanel->raise();
}

void MainWindow::setSystemTray(SystemTray *tray)
{
    m_systemTray = tray;
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
        const QString baseName = "Qlippy_" + lang;
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

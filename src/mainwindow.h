#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPoint>
#include <QTimer>
#include <QMenu>

#include "ConfigManager.h"

class SpriteAnimationEngine;
class LottieAnimationEngine;
#ifdef OAI_LIVE2D_SUPPORT
class Live2DAnimationEngine;
#endif
#ifdef OAI_TTS_ENABLED
class TTSEngine;
#endif
class TipWidget;
class SettingsPanelWidget;
class EcgWidget;
class CharacterPackManager;
class EventRouter;
class FullscreenWatcher;
class PetStateMachine;
class GlobalShortcutManager;

class QTranslator;
class SystemTray;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(ConfigManager *config, QTranslator *translator, QWidget *parent = nullptr);
    ~MainWindow() override;

    SpriteAnimationEngine *animationEngine() const { return m_engine; }
    LottieAnimationEngine *lottieEngine() const { return m_lottieEngine; }
#ifdef OAI_LIVE2D_SUPPORT
    Live2DAnimationEngine *live2dEngine() const { return m_live2dEngine; }
#endif
    TipWidget *tipWidget() const { return m_tipWidget; }
    EcgWidget *ecgWidget() const { return m_ecgWidget; }

    void setSystemTray(SystemTray *tray);
    void setCharacterPackManager(CharacterPackManager *manager);
    void setGlobalShortcutManager(GlobalShortcutManager *manager);
    void setEventRouter(EventRouter *router) { m_eventRouter = router; }
    void setStateMachine(PetStateMachine *sm) { m_stateMachine = sm; }

signals:
    void positionChanged(const QPoint &pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

public slots:
    void retranslateUi();
    void onLanguageChanged(const QString &lang);
    void showContextMenu(const QPoint &globalPos);

private slots:
    void toggleVisibility();
    void openSettings();
    void onActivePackChanged();
    void onDisplayModeChanged(ConfigManager::DisplayMode mode);
    void onFullscreenStarted();
    void onFullscreenStopped();

private:
    void setupWindowFlags();
    void reloadTranslator(const QString &lang);
    void showRandomGreeting();

    QRect petRect() const;
    bool isInPetRect(const QPoint &pos) const;
    bool isValidCodexPet(const QString &filePath) const;

    SpriteAnimationEngine *m_engine;
    LottieAnimationEngine *m_lottieEngine;
#ifdef OAI_LIVE2D_SUPPORT
    Live2DAnimationEngine *m_live2dEngine = nullptr;
#endif
#ifdef OAI_TTS_ENABLED
    TTSEngine *m_ttsEngine = nullptr;
#endif
    ConfigManager *m_config;
    TipWidget *m_tipWidget;
    SettingsPanelWidget *m_settingsPanel;
    EcgWidget *m_ecgWidget = nullptr;
    QTranslator *m_translator;
    SystemTray *m_systemTray = nullptr;
    CharacterPackManager *m_packManager = nullptr;
    EventRouter *m_eventRouter = nullptr;
    PetStateMachine *m_stateMachine = nullptr;
    GlobalShortcutManager *m_shortcutManager = nullptr;

    // Gaming Mode
    FullscreenWatcher *m_fullscreenWatcher = nullptr;
    bool m_hiddenByGamingMode = false;

    // Drag state
    QPoint m_dragStartPos;
    QPoint m_dragWindowPos;
    bool m_dragging = false;
    static constexpr int DRAG_THRESHOLD = 5;

    // Active pack's render size (drives petRect). Defaults to Clippy's
    // historical 124×93; onActivePackChanged() updates it to the pack's
    // frameWidth/frameHeight so Live2D/Lottie packs aren't squished into
    // a Clippy-sized rect at the bottom of a much taller window.
    QSize m_petSize = QSize(124, 93);

    // Visibility
    bool m_visible = true;

    // Guard against infinite recursion when auto-skipping Live2D packs
    // (onActivePackChanged → switchPack → activePackChanged → onActivePackChanged).
    bool m_skipLive2dFallback = false;

    // Monotonic counter for pack load attempts. Each onActivePackChanged()
    // bumps it; the 500 ms post-load crop lambda compares its captured value
    // against the current one and bails if it's been superseded.
    int m_packLoadId = 0;

#ifdef Q_OS_WIN
    // Windows DWM can lose window attributes after long-running sessions
    // (display sleep/wake, DWM restart). Refresh periodically.
    QTimer *m_dwmRefreshTimer = nullptr;
#endif
};

#endif // MAINWINDOW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPoint>
#include <QTimer>
#include <QMenu>

class SpriteAnimationEngine;
class LottieAnimationEngine;
#ifdef OAI_LIVE2D_SUPPORT
class Live2DAnimationEngine;
#endif
class ConfigManager;
class TipBubbleWidget;
class SettingsPanelWidget;
class CharacterPackManager;

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
    TipBubbleWidget *tipBubbleWidget() const { return m_tipBubble; }

    void setSystemTray(SystemTray *tray);
    void setCharacterPackManager(CharacterPackManager *manager);

signals:
    void positionChanged(const QPoint &pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public slots:
    void retranslateUi();
    void onLanguageChanged(const QString &lang);

private slots:
    void toggleVisibility();
    void openSettings();
    void onActivePackChanged();

private:
    void setupWindowFlags();
    void reloadTranslator(const QString &lang);
    void showRandomGreeting();

    QRect petRect() const;
    bool isInPetRect(const QPoint &pos) const;

    SpriteAnimationEngine *m_engine;
    LottieAnimationEngine *m_lottieEngine;
#ifdef OAI_LIVE2D_SUPPORT
    Live2DAnimationEngine *m_live2dEngine = nullptr;
#endif
    ConfigManager *m_config;
    TipBubbleWidget *m_tipBubble;
    SettingsPanelWidget *m_settingsPanel;
    QTranslator *m_translator;
    SystemTray *m_systemTray = nullptr;
    CharacterPackManager *m_packManager = nullptr;

    // Drag state
    QPoint m_dragStartPos;
    QPoint m_dragWindowPos;
    bool m_dragging = false;
    static constexpr int DRAG_THRESHOLD = 5;

    // Visibility
    bool m_visible = true;
};

#endif // MAINWINDOW_H

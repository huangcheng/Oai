#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPoint>
#include <QTimer>
#include <QMenu>

class SpriteAnimationEngine;
class SpeechBubble;
class LottieEffectOverlay;
class ConfigManager;
class TipBubbleWidget;
class SettingsPanelWidget;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(ConfigManager *config, QWidget *parent = nullptr);
    ~MainWindow() override;

    SpriteAnimationEngine *animationEngine() const { return m_engine; }
    SpeechBubble *speechBubble() const { return m_bubble; }
    LottieEffectOverlay *effectOverlay() const { return m_effects; }
    TipBubbleWidget *tipBubbleWidget() const { return m_tipBubble; }

signals:
    void positionChanged(const QPoint &pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void toggleVisibility();
    void openSettings();

private:
    void setupWindowFlags();

    SpriteAnimationEngine *m_engine;
    SpeechBubble *m_bubble;
    LottieEffectOverlay *m_effects;
    ConfigManager *m_config;
    TipBubbleWidget *m_tipBubble;
    SettingsPanelWidget *m_settingsPanel;

    // Drag state
    QPoint m_dragStartPos;
    QPoint m_dragWindowPos;
    bool m_dragging = false;
    static constexpr int DRAG_THRESHOLD = 5;

    // Visibility
    bool m_visible = true;
};

#endif // MAINWINDOW_H

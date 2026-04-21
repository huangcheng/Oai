#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPoint>
#include <QTimer>
#include <QMenu>

class SpriteAnimationEngine;
class LottieEffectOverlay;
class ConfigManager;
class TipBubbleWidget;
class SettingsPanelWidget;

class QTranslator;
class SystemTray;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(ConfigManager *config, QTranslator *translator, QWidget *parent = nullptr);
    ~MainWindow() override;

    SpriteAnimationEngine *animationEngine() const { return m_engine; }
    LottieEffectOverlay *effectOverlay() const { return m_effects; }
    TipBubbleWidget *tipBubbleWidget() const { return m_tipBubble; }

    void setSystemTray(SystemTray *tray);

signals:
    void positionChanged(const QPoint &pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

public slots:
    void retranslateUi();
    void onLanguageChanged(const QString &lang);

private slots:
    void toggleVisibility();
    void openSettings();

private:
    void setupWindowFlags();
    void reloadTranslator(const QString &lang);
    void showRandomGreeting();

    QRect petRect() const;
    bool isInPetRect(const QPoint &pos) const;

    SpriteAnimationEngine *m_engine;
    LottieEffectOverlay *m_effects;
    ConfigManager *m_config;
    TipBubbleWidget *m_tipBubble;
    SettingsPanelWidget *m_settingsPanel;
    QTranslator *m_translator;
    SystemTray *m_systemTray = nullptr;

    // Drag state
    QPoint m_dragStartPos;
    QPoint m_dragWindowPos;
    bool m_dragging = false;
    static constexpr int DRAG_THRESHOLD = 5;



    // Visibility
    bool m_visible = true;
};

#endif // MAINWINDOW_H

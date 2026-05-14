#ifndef SETTINGSPANELWIDGET_H
#define SETTINGSPANELWIDGET_H

#include <QWidget>
#include <QString>
#include <QPropertyAnimation>

class ConfigManager;
class CharacterPackManager;

class QLabel;
class QPushButton;
class QFrame;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QToolButton;
class QAction;
class QKeySequenceEdit;

class SettingsPanelWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal panelScale READ panelScale WRITE setPanelScale)
    Q_PROPERTY(qreal panelOpacity READ panelOpacity WRITE setPanelOpacity)

public:
    explicit SettingsPanelWidget(ConfigManager *config, QWidget *parent = nullptr);

    // Position relative to the pet widget
    void anchorTo(const QWidget *petWidget);
    void setAnchorRect(const QRect &rect) { m_anchorRect = rect; }

    // Set sprite pack manager for pack selection
    void setCharacterPackManager(CharacterPackManager *manager);

    // Retranslate UI when language changes at runtime
    void retranslateUi();

    // Animated show/hide
    void showAnimated();
    void hideAnimated();

    qreal panelScale() const { return m_scale; }
    void setPanelScale(qreal s);
    qreal panelOpacity() const { return m_panelOpacity; }
    void setPanelOpacity(qreal o);

signals:
    void panelHidden();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onCloseClicked();
    void onTabChanged(int tabIndex);
    void onLanguageChanged(int index);
    void onAutoStartToggled(bool checked);
    void onModeChanged(int index);
    void onPortEditingFinished();
    void onShortcutChanged(const QKeySequence &sequence);
    void onGamingModeToggled(bool checked);
    void onTipBubblesToggled(bool checked);
#ifdef OAI_TTS_ENABLED
    void onTtsEnabledToggled(bool checked);
    void onTtsBaseUrlChanged(const QString &text);
    void onTtsTokenChanged(const QString &text);
    void onTtsModelChanged(const QString &text);
    void onTtsLanguageChanged(int index);
#endif

private:
    void setupUi();
    void positionRelativeTo(const QWidget *pet);
    void refreshPackList();
    void updatePackButtonLabel();
    void updatePackRowVisibility();

    ConfigManager *m_config;
    CharacterPackManager *m_packManager = nullptr;

    // UI elements
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_closeButton = nullptr;
    QFrame *m_separator = nullptr;
    QLabel *m_langLabel = nullptr;
    QComboBox *m_langCombo = nullptr;
    QLabel *m_autoStartLabel = nullptr;
    QCheckBox *m_autoStartCheck = nullptr;
    QLabel *m_modeLabel = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QLabel *m_portLabel = nullptr;
    QLineEdit *m_portInput = nullptr;
    QLabel *m_packLabel = nullptr;
    QToolButton *m_packButton = nullptr;
    QLabel *m_shortcutLabel = nullptr;
    QKeySequenceEdit *m_shortcutEdit = nullptr;
    QLabel *m_gamingModeLabel = nullptr;
    QCheckBox *m_gamingModeCheck = nullptr;
    QLabel *m_tipBubblesLabel = nullptr;
    QCheckBox *m_tipBubblesCheck = nullptr;

    // Tab buttons (left side)
    QPushButton *m_generalTabBtn = nullptr;
    QPushButton *m_aiTabBtn = nullptr;

    // Tab content containers
    QWidget *m_generalTab = nullptr;
    QWidget *m_aiTab = nullptr;

#ifdef OAI_TTS_ENABLED
    // TTS UI elements
    QLabel *m_ttsEnabledLabel = nullptr;
    QCheckBox *m_ttsEnabledCheck = nullptr;
    QLabel *m_ttsBaseUrlLabel = nullptr;
    QLineEdit *m_ttsBaseUrlInput = nullptr;
    QLabel *m_ttsTokenLabel = nullptr;
    QLineEdit *m_ttsTokenInput = nullptr;
    QLabel *m_ttsModelLabel = nullptr;
    QLineEdit *m_ttsModelInput = nullptr;
    QLabel *m_ttsLanguageLabel = nullptr;
    QComboBox *m_ttsLanguageCombo = nullptr;
#endif

    // Layout container
    QWidget *m_contentWidget = nullptr;
    QRect m_anchorRect;  // rect within the anchored widget to anchor to (empty = full widget)

    // Animation
    qreal m_scale = 1.0;
    qreal m_panelOpacity = 1.0;
    QPropertyAnimation *m_scaleAnim = nullptr;
    QPropertyAnimation *m_opacityAnim = nullptr;

    // Styling constants
    static constexpr int PADDING = 14;
    static constexpr int VERTICAL_SPACING = 12;
    static constexpr int SHADOW_BLUR = 10;
    static constexpr int CORNER_RADIUS = 4;
    static constexpr int BORDER_WIDTH = 3;
    static constexpr int SKEW_PX = 4;
    static constexpr int PANEL_WIDTH = 300;
    static constexpr int PANEL_HEIGHT = 310;
};

#endif // SETTINGSPANELWIDGET_H

#ifndef SETTINGSPANELWIDGET_H
#define SETTINGSPANELWIDGET_H

#include <QWidget>
#include <QString>

class ConfigManager;

class QLabel;
class QPushButton;
class QFrame;
class QComboBox;
class QCheckBox;
class QLineEdit;

class SettingsPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanelWidget(ConfigManager *config, QWidget *parent = nullptr);

    // Position relative to the pet widget
    void anchorTo(const QWidget *petWidget);
    void setAnchorRect(const QRect &rect) { m_anchorRect = rect; }

    // Retranslate UI when language changes at runtime
    void retranslateUi();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onCloseClicked();
    void onLanguageChanged(int index);
    void onAutoStartToggled(bool checked);
    void onPortEditingFinished();

private:
    void setupUi();
    void positionRelativeTo(const QWidget *pet);

    ConfigManager *m_config;

    // UI elements
    QLabel *m_titleLabel = nullptr;
    QPushButton *m_closeButton = nullptr;
    QFrame *m_separator = nullptr;
    QLabel *m_langLabel = nullptr;
    QComboBox *m_langCombo = nullptr;
    QLabel *m_autoStartLabel = nullptr;
    QCheckBox *m_autoStartCheck = nullptr;
    QLabel *m_portLabel = nullptr;
    QLineEdit *m_portInput = nullptr;

    // Layout container
    QWidget *m_contentWidget = nullptr;
    QRect m_anchorRect;  // rect within the anchored widget to anchor to (empty = full widget)

    // Styling constants
    static constexpr int PADDING = 12;
    static constexpr int VERTICAL_SPACING = 10;
    static constexpr int SHADOW_OFFSET = 4;
    static constexpr int PANEL_WIDTH = 180;
    static constexpr int PANEL_HEIGHT = 150;
};

#endif // SETTINGSPANELWIDGET_H
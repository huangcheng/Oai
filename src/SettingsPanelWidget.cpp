#include "SettingsPanelWidget.h"
#include "ConfigManager.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QComboBox>
#include <QCheckBox>
#include <QScreen>
#include <QGuiApplication>
#include <QFont>

SettingsPanelWidget::SettingsPanelWidget(ConfigManager *config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    setWindowFlags(
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool |
        Qt::WindowDoesNotAcceptFocus
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    setFixedSize(PANEL_WIDTH + SHADOW_OFFSET, PANEL_HEIGHT + SHADOW_OFFSET);

    setupUi();

    // Read initial state from ConfigManager
    QString lang = m_config->language();
    int langIndex = (lang == "zh_CN") ? 1 : 0;
    m_langCombo->setCurrentIndex(langIndex);

    bool autoStart = m_config->autoStart();
    m_autoStartCheck->setChecked(autoStart);
}

void SettingsPanelWidget::anchorTo(const QWidget *petWidget)
{
    if (petWidget && isVisible()) {
        positionRelativeTo(petWidget);
    }
}

void SettingsPanelWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Draw shadow (solid black, no blur, 4px offset)
    painter.fillRect(SHADOW_OFFSET, SHADOW_OFFSET,
                     PANEL_WIDTH, PANEL_HEIGHT,
                     Qt::black);

    // Draw panel background (#FFFFE1)
    QPainterPath path;
    path.addRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT);
    painter.fillPath(path, QColor(255, 255, 225));

    // Draw panel border (1px black)
    QPen borderPen(Qt::black, 1);
    painter.setPen(borderPen);
    painter.drawRect(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1);
}

void SettingsPanelWidget::setupUi()
{
    // Create content widget that sits inside the panel area
    m_contentWidget = new QWidget(this);
    m_contentWidget->setGeometry(0, 0, PANEL_WIDTH, PANEL_HEIGHT);
    m_contentWidget->setStyleSheet("background: transparent;");

    // Main vertical layout for content
    QVBoxLayout *mainLayout = new QVBoxLayout(m_contentWidget);
    mainLayout->setContentsMargins(PADDING, PADDING, PADDING, PADDING);
    mainLayout->setSpacing(VERTICAL_SPACING);

    // Title row: "Settings" label + close button
    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(4);

    m_titleLabel = new QLabel(tr("Settings"), m_contentWidget);
    QFont titleFont("Tahoma", 10, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet("color: black; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_closeButton = new QPushButton(tr("×"), m_contentWidget);
    m_closeButton->setFont(QFont("Tahoma", 12, QFont::Bold));
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            color: black;
            padding: 0px;
        }
        QPushButton:hover {
            background: #CCCCCC;
        }
    )");
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsPanelWidget::onCloseClicked);

    titleRow->addWidget(m_titleLabel, 1);
    titleRow->addWidget(m_closeButton);

    // Separator line
    m_separator = new QFrame(m_contentWidget);
    m_separator->setFrameShape(QFrame::HLine);
    m_separator->setFrameShadow(QFrame::Plain);
    m_separator->setStyleSheet("border: none; border-top: 1px solid black; background: transparent;");
    m_separator->setFixedHeight(1);

    // Language row: label + combo
    QHBoxLayout *langRow = new QHBoxLayout();
    langRow->setSpacing(8);

    m_langLabel = new QLabel(tr("Language"), m_contentWidget);
    m_langLabel->setFont(QFont("Tahoma", 9));
    m_langLabel->setStyleSheet("color: black; background: transparent;");
    m_langLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_langCombo = new QComboBox(m_contentWidget);
    m_langCombo->addItem(tr("English"), "en");
    m_langCombo->addItem(tr("简体中文"), "zh_CN");
    m_langCombo->setFont(QFont("Tahoma", 9));
    m_langCombo->setFixedHeight(20);
    m_langCombo->setStyleSheet(R"(
        QComboBox {
            background: white;
            border: 1px solid black;
            padding: 1px 4px;
            min-width: 70px;
        }
        QComboBox::drop-down {
            border-left: 1px solid black;
            width: 18px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 3px solid transparent;
            border-right: 3px solid transparent;
            border-top: 5px solid black;
            margin-right: 4px;
        }
    )");
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPanelWidget::onLanguageChanged);

    langRow->addWidget(m_langLabel, 1);
    langRow->addWidget(m_langCombo, 0);

    // Auto-start row: label + checkbox
    QHBoxLayout *autoStartRow = new QHBoxLayout();
    autoStartRow->setSpacing(8);

    m_autoStartLabel = new QLabel(tr("Launch at Login"), m_contentWidget);
    m_autoStartLabel->setFont(QFont("Tahoma", 9));
    m_autoStartLabel->setStyleSheet("color: black; background: transparent;");
    m_autoStartLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_autoStartCheck = new QCheckBox(m_contentWidget);
    m_autoStartCheck->setFixedSize(14, 14);
    m_autoStartCheck->setStyleSheet(R"(
        QCheckBox::indicator {
            width: 10px;
            height: 10px;
            background: white;
            border: 1px solid black;
        }
        QCheckBox::indicator:checked {
            background: black;
        }
        QCheckBox::indicator:unchecked {
            background: white;
        }
    )");
    connect(m_autoStartCheck, &QCheckBox::toggled,
            this, &SettingsPanelWidget::onAutoStartToggled);

    autoStartRow->addWidget(m_autoStartLabel, 1);
    autoStartRow->addWidget(m_autoStartCheck, 0, Qt::AlignLeft);

    // Add all rows to main layout
    mainLayout->addLayout(titleRow);
    mainLayout->addWidget(m_separator);
    mainLayout->addLayout(langRow);
    mainLayout->addLayout(autoStartRow);
    mainLayout->addStretch(1);
}

void SettingsPanelWidget::positionRelativeTo(const QWidget *pet)
{
    if (!pet)
        return;

    QRect anchor = m_anchorRect.isValid() ? m_anchorRect : QRect(0, 0, pet->width(), pet->height());
    QPoint petTopLeft = pet->mapToGlobal(anchor.topLeft());
    int petCenterX = petTopLeft.x() + anchor.width() / 2;
    int petTop = petTopLeft.y();
    int petBottom = petTop + anchor.height();

    // Default position: above the pet
    int panelX = petCenterX - PANEL_WIDTH / 2;
    int panelY = petTop - PANEL_HEIGHT - 5; // 5px gap

    // Check if we need to flip below
    QScreen *screen = QGuiApplication::screenAt(pet->mapToGlobal(QPoint(pet->width() / 2, pet->height() / 2)));
    if (screen) {
        QRect screenRect = screen->availableGeometry();

        // If not enough room above, flip to below
        if (panelY < screenRect.top()) {
            panelY = petBottom + 5;
        }

        // Clamp to screen edges
        panelX = qBound(screenRect.left(), panelX, screenRect.right() - PANEL_WIDTH);
        panelY = qBound(screenRect.top(), panelY, screenRect.bottom() - PANEL_HEIGHT);
    }

    // Account for shadow offset
    move(panelX - SHADOW_OFFSET, panelY - SHADOW_OFFSET);
}

void SettingsPanelWidget::onCloseClicked()
{
    hide();
}

void SettingsPanelWidget::onLanguageChanged(int index)
{
    QString langCode = m_langCombo->itemData(index).toString();
    m_config->setLanguage(langCode);
    m_config->save();
}

void SettingsPanelWidget::onAutoStartToggled(bool checked)
{
    m_config->setAutoStart(checked);
    m_config->save();
}

void SettingsPanelWidget::retranslateUi()
{
    m_titleLabel->setText(tr("Settings"));
    m_closeButton->setText(tr("×"));
    m_langLabel->setText(tr("Language"));
    m_langCombo->setItemText(0, tr("English"));
    m_langCombo->setItemText(1, tr("简体中文"));
    m_autoStartLabel->setText(tr("Launch at Login"));
}
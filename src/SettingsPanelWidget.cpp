#include "SettingsPanelWidget.h"
#include "ConfigManager.h"
#include "CharacterPackManager.h"
#include "CharacterPack.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QScreen>
#include <QGuiApplication>
#include <QFont>
#include <QPixmap>
#include <QTemporaryDir>
#include <QDir>
#include <QStandardPaths>
#include <QPolygon>
#include <QFile>
#include <QListView>

SettingsPanelWidget::SettingsPanelWidget(ConfigManager *config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    setWindowFlags(
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::WindowDoesNotAcceptFocus
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif

    setFixedSize(PANEL_WIDTH + SHADOW_BLUR * 2, PANEL_HEIGHT + SHADOW_BLUR * 2);

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
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body(SHADOW_BLUR, SHADOW_BLUR, PANEL_WIDTH, PANEL_HEIGHT);
    const qreal r = CORNER_RADIUS;
    const qreal sk = SKEW_PX;

    // Build skewed panel path (matching tip bubble parallelogram)
    QPainterPath panelPath;
    panelPath.moveTo(body.left() + sk + r, body.top());
    panelPath.lineTo(body.right() + sk - r, body.top());
    panelPath.quadTo(body.right() + sk, body.top(), body.right() + sk, body.top() + r);
    panelPath.lineTo(body.right(), body.bottom() - r);
    panelPath.quadTo(body.right(), body.bottom(), body.right() - r, body.bottom());
    panelPath.lineTo(body.left() + r, body.bottom());
    panelPath.quadTo(body.left(), body.bottom(), body.left(), body.bottom() - r);
    panelPath.lineTo(body.left() + sk, body.top() + r);
    panelPath.quadTo(body.left() + sk, body.top(), body.left() + sk + r, body.top());
    panelPath.closeSubpath();

    // Bold shadow
    painter.save();
    painter.setOpacity(0.35);
    painter.setPen(Qt::NoPen);
    QPainterPath shadowPath = panelPath;
    shadowPath.translate(3, 4);
    painter.setBrush(Qt::black);
    painter.drawPath(shadowPath);
    painter.restore();

    // White fill + thick black border
    painter.setPen(QPen(Qt::black, BORDER_WIDTH, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(Qt::white);
    painter.drawPath(panelPath);

    // Red accent stripe at top
    painter.save();
    painter.setClipPath(panelPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xE0, 0x1A, 0x2B));
    painter.drawRect(QRectF(body.left(), body.top(), body.width() + sk, 4));
    painter.restore();
}

void SettingsPanelWidget::setupUi()
{
    // Create content widget that sits inside the panel area
    m_contentWidget = new QWidget(this);
    m_contentWidget->setGeometry(SHADOW_BLUR, SHADOW_BLUR, PANEL_WIDTH, PANEL_HEIGHT);
    m_contentWidget->setStyleSheet("background: transparent;");

    // Main vertical layout for content
    QVBoxLayout *mainLayout = new QVBoxLayout(m_contentWidget);
    mainLayout->setContentsMargins(PADDING, PADDING, PADDING, PADDING);
    mainLayout->setSpacing(VERTICAL_SPACING);

    // Title row: "Settings" label + close button
    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(4);

    m_titleLabel = new QLabel(tr("Settings"), m_contentWidget);
    QFont titleFont("HarmonyOS Sans SC", 10, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet("color: black; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_closeButton = new QPushButton(tr("×"), m_contentWidget);
    m_closeButton->setFont(QFont("HarmonyOS Sans SC", 12, QFont::Bold));
    m_closeButton->setFixedSize(22, 22);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            border-radius: 3px;
            color: #888;
            padding: 0px;
        }
        QPushButton:hover {
            background: #E01A2B;
            color: white;
        }
    )");
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsPanelWidget::onCloseClicked);

    titleRow->addWidget(m_titleLabel, 1);
    titleRow->addWidget(m_closeButton);

    // Separator line
    m_separator = new QFrame(m_contentWidget);
    m_separator->setFrameShape(QFrame::HLine);
    m_separator->setFrameShadow(QFrame::Plain);
    m_separator->setStyleSheet("border: none; border-top: 2px solid black; background: transparent;");
    m_separator->setFixedHeight(1);

    // Language row: label + combo
    m_langLabel = new QLabel(tr("Language"), m_contentWidget);
    m_langLabel->setFont(QFont("HarmonyOS Sans SC", 10));
    m_langLabel->setStyleSheet("color: black; background: transparent;");
    m_langLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_langCombo = new QComboBox(m_contentWidget);
    // Force Qt-drawn popup instead of native macOS popup (native ignores stylesheets)
    auto *listView = new QListView(m_langCombo);
    listView->setFont(QFont("HarmonyOS Sans SC", 10));
    m_langCombo->setView(listView);
    m_langCombo->addItem(tr("English"), "en");
    m_langCombo->addItem(tr("简体中文"), "zh_CN");
    m_langCombo->setFont(QFont("HarmonyOS Sans SC", 10));
    m_langCombo->setFixedHeight(24);

    // Generate a small down-arrow pixmap
    QString arrowPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/oai_combo_arrow.png";
    if (!QFile::exists(arrowPath)) {
        QPixmap arrow(8, 5);
        arrow.fill(Qt::transparent);
        QPainter p(&arrow);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(Qt::black);
        p.setPen(Qt::NoPen);
        QPolygon tri;
        tri << QPoint(0, 0) << QPoint(8, 0) << QPoint(4, 5);
        p.drawPolygon(tri);
        p.end();
        arrow.save(arrowPath);
    }

    m_langCombo->setStyleSheet(QStringLiteral(R"(
        QComboBox {
            background: white;
            border: 2px solid black;
            border-radius: 3px;
            padding: 2px 6px;
            color: #2C2C2E;
            min-width: 70px;
        }
        QComboBox::drop-down {
            border-left: 2px solid black;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            width: 18px;
            subcontrol-origin: padding;
            subcontrol-position: center right;
        }
        QComboBox::down-arrow {
            image: url(%1);
            width: 8px;
            height: 5px;
        }
        QComboBox QAbstractItemView {
            background: white;
            color: #2C2C2E;
            border: 2px solid black;
            border-radius: 4px;
            selection-background-color: #E01A2B;
            selection-color: white;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            color: #2C2C2E;
            padding: 3px 6px;
        }
        QComboBox QAbstractItemView::item:selected {
            background: #E01A2B;
            color: white;
        }
    )").arg(arrowPath));
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPanelWidget::onLanguageChanged);

    // Auto-start row: label + checkbox
    m_autoStartLabel = new QLabel(tr("Launch at Login"), m_contentWidget);
    m_autoStartLabel->setFont(QFont("HarmonyOS Sans SC", 10));
    m_autoStartLabel->setStyleSheet("color: black; background: transparent;");
    m_autoStartLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_autoStartCheck = new QCheckBox(m_contentWidget);
    m_autoStartCheck->setFixedSize(16, 16);
    m_autoStartCheck->setStyleSheet(R"(
        QCheckBox::indicator {
            width: 12px;
            height: 12px;
            background: white;
            border: 2px solid black;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background: #E01A2B;
            border: 1px solid #E01A2B;
        }
        QCheckBox::indicator:unchecked {
            background: white;
        }
    )");
    connect(m_autoStartCheck, &QCheckBox::toggled,
            this, &SettingsPanelWidget::onAutoStartToggled);

    // Port row: label + input
    m_portLabel = new QLabel(tr("Port"), m_contentWidget);
    m_portLabel->setFont(QFont("HarmonyOS Sans SC", 10));
    m_portLabel->setStyleSheet("color: black; background: transparent;");
    m_portLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_portInput = new QLineEdit(m_contentWidget);
    m_portInput->setFont(QFont("HarmonyOS Sans SC", 10));
    m_portInput->setText(QString::number(m_config->ipcPort()));
    m_portInput->setMaxLength(5);
    m_portInput->setFixedHeight(24);
    m_portInput->setStyleSheet(R"(
        QLineEdit {
            background: white;
            border: 2px solid black;
            border-radius: 3px;
            padding: 2px 6px;
            color: #2C2C2E;
        }
    )");
    connect(m_portInput, &QLineEdit::editingFinished,
            this, &SettingsPanelWidget::onPortEditingFinished);

    // Pack selection row: label + combo
    m_packLabel = new QLabel(tr("Pet"), m_contentWidget);
    m_packLabel->setFont(QFont("HarmonyOS Sans SC", 10));
    m_packLabel->setStyleSheet("color: black; background: transparent;");
    m_packLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_packCombo = new QComboBox(m_contentWidget);
    auto *packListView = new QListView(m_packCombo);
    packListView->setFont(QFont("HarmonyOS Sans SC", 10));
    m_packCombo->setView(packListView);
    m_packCombo->setFont(QFont("HarmonyOS Sans SC", 10));
    m_packCombo->setFixedHeight(24);
    m_packCombo->setStyleSheet(QStringLiteral(R"(
        QComboBox {
            background: white;
            border: 2px solid black;
            border-radius: 3px;
            padding: 2px 6px;
            color: #2C2C2E;
            min-width: 70px;
        }
        QComboBox::drop-down {
            border-left: 2px solid black;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            width: 18px;
            subcontrol-origin: padding;
            subcontrol-position: center right;
        }
        QComboBox::down-arrow {
            image: url(%1);
            width: 8px;
            height: 5px;
        }
        QComboBox QAbstractItemView {
            background: white;
            color: #2C2C2E;
            border: 2px solid black;
            border-radius: 4px;
            selection-background-color: #E01A2B;
            selection-color: white;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            color: #2C2C2E;
            padding: 3px 6px;
        }
        QComboBox QAbstractItemView::item:selected {
            background: #E01A2B;
            color: white;
        }
    )").arg(arrowPath));
    connect(m_packCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPanelWidget::onPackChanged);

    // Grid layout for form rows: labels in col 0, controls in col 1
    QGridLayout *formGrid = new QGridLayout();
    formGrid->setHorizontalSpacing(10);
    formGrid->setVerticalSpacing(VERTICAL_SPACING);
    formGrid->setColumnStretch(1, 1);  // controls column stretches

    formGrid->addWidget(m_langLabel,       0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_langCombo,       0, 1);
    formGrid->addWidget(m_autoStartLabel,  1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_autoStartCheck,  1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_portLabel,       2, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_portInput,       2, 1);
    formGrid->addWidget(m_packLabel,       3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_packCombo,       3, 1);

    // Add all rows to main layout
    mainLayout->addLayout(titleRow);
    mainLayout->addWidget(m_separator);
    mainLayout->addLayout(formGrid);
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

    // Account for shadow margin
    move(panelX - SHADOW_BLUR, panelY - SHADOW_BLUR);
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

void SettingsPanelWidget::onPortEditingFinished()
{
    const QString text = m_portInput->text().trimmed();
    bool ok;
    int port = text.toInt(&ok);
    if (ok && port >= 1024 && port <= 65535) {
        m_config->setIpcPort(static_cast<quint16>(port));
    } else {
        // Revert to current config value
        m_portInput->setText(QString::number(m_config->ipcPort()));
    }
}

void SettingsPanelWidget::setCharacterPackManager(CharacterPackManager *manager)
{
    m_packManager = manager;
    refreshPackList();
}

void SettingsPanelWidget::onPackChanged(int index)
{
    if (!m_packManager || index < 0) {
        return;
    }

    QString packId = m_packCombo->itemData(index).toString();
    if (!packId.isEmpty()) {
        m_packManager->switchPack(packId);
    }
}

void SettingsPanelWidget::refreshPackList()
{
    if (!m_packCombo) {
        return;
    }

    // Block currentIndexChanged while populating. Without this, the first
    // addItem() fires currentIndexChanged(0) which calls onPackChanged ->
    // switchPack(firstPack) and clobbers the pack that was just restored
    // from config at startup.
    const QSignalBlocker blocker(m_packCombo);

    m_packCombo->clear();

    if (!m_packManager) {
        return;
    }

    const auto packs = m_packManager->availablePacks();
    for (const auto &pack : packs) {
        m_packCombo->addItem(pack.name, pack.id);
    }

    // Select active pack
    QString activeId = m_packManager->activePackId();
    if (!activeId.isEmpty()) {
        int index = m_packCombo->findData(activeId);
        if (index >= 0) {
            m_packCombo->setCurrentIndex(index);
        }
    }
}

void SettingsPanelWidget::retranslateUi()
{
    m_titleLabel->setText(tr("Settings"));
    m_closeButton->setText(tr("×"));
    m_langLabel->setText(tr("Language"));
    m_langCombo->setItemText(0, tr("English"));
    m_langCombo->setItemText(1, tr("简体中文"));
    m_autoStartLabel->setText(tr("Launch at Login"));
    m_portLabel->setText(tr("Port"));
    m_packLabel->setText(tr("Pet"));
}
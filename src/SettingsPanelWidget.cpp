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
#include <QShowEvent>
#include <QStyle>
#include <QStyleOptionButton>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#endif
#include <QTemporaryDir>
#include <QDir>
#include <QStandardPaths>
#include <QPolygon>
#include <QFile>
#include <QListView>
#include <QToolButton>
#include <QMenu>
#include <QActionGroup>
#include <QTransform>

// All UI fonts in this panel are HarmonyOS Sans SC. The panel is translucent,
// so Windows can't apply ClearType subpixel AA — Qt falls back to grayscale.
// PreferAntialias keeps glyphs smoothed regardless of subpixel-positioning
// quirks; PreferNoHinting avoids stroke-snapping that mangles CJK glyphs at
// small point sizes.
static QFont harmonyFont(int pointSize, QFont::Weight weight = QFont::Normal)
{
    QFont f(QStringLiteral("HarmonyOS Sans SC"), pointSize, weight);
    f.setStyleStrategy(QFont::PreferAntialias);
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

namespace {
// QSS can color the indicator box but cannot draw the tick glyph. Override
// paintEvent to overlay a checkmark on top of the styled box when checked.
class CheckMarkBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    void paintEvent(QPaintEvent *e) override {
        QCheckBox::paintEvent(e);
        if (!isChecked()) return;
        QStyleOptionButton opt;
        initStyleOption(&opt);
        const QRect r = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &opt, this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(Qt::white);
        pen.setWidthF(1.8);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        const qreal x = r.x();
        const qreal y = r.y();
        const qreal w = r.width();
        const qreal h = r.height();
        QPainterPath path;
        path.moveTo(x + w * 0.22, y + h * 0.52);
        path.lineTo(x + w * 0.42, y + h * 0.72);
        path.lineTo(x + w * 0.78, y + h * 0.32);
        p.drawPath(path);
    }
};
}

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
    // Same Win11 DWM workaround as MainWindow / TipBubbleWidget — without
    // WA_NoSystemBackground the system fills the panel's window with white
    // before paintEvent runs, and DWM then draws rounded corners + shadow
    // + Mica around it.
    setAttribute(Qt::WA_NoSystemBackground, true);
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

    m_ecgCheck->setChecked(m_config->ecgEnabled());
}

void SettingsPanelWidget::anchorTo(const QWidget *petWidget)
{
    if (petWidget) {
        positionRelativeTo(petWidget);
    }
}

void SettingsPanelWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        const int doNotRound = 1;          // DWMWCP_DONOTROUND
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &doNotRound, sizeof(doNotRound));
        const int backdropNone = 1;        // DWMSBT_NONE
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                              &backdropNone, sizeof(backdropNone));
        const int ncRenderingDisabled = 1; // DWMNCRP_DISABLED
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                              &ncRenderingDisabled, sizeof(ncRenderingDisabled));
    }
#endif
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
    painter.setBrush(QColor(0xF3, 0x6F, 0x1A));
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
    m_titleLabel->setFont(harmonyFont(10, QFont::Bold));
    m_titleLabel->setStyleSheet("color: black; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_closeButton = new QPushButton(tr("×"), m_contentWidget);
    m_closeButton->setFont(harmonyFont(12, QFont::Bold));
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
            background: #F36F1A;
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
    m_langLabel->setFont(harmonyFont(10));
    m_langLabel->setStyleSheet("color: black; background: transparent;");
    m_langLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_langCombo = new QComboBox(m_contentWidget);
    // Force Qt-drawn popup instead of native macOS popup (native ignores stylesheets)
    auto *listView = new QListView(m_langCombo);
    listView->setFont(harmonyFont(10));
    m_langCombo->setView(listView);
    m_langCombo->addItem(tr("English"), "en");
    m_langCombo->addItem(tr("简体中文"), "zh_CN");
    m_langCombo->setFont(harmonyFont(10));
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
            selection-background-color: #F36F1A;
            selection-color: white;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            color: #2C2C2E;
            padding: 3px 6px;
        }
        QComboBox QAbstractItemView::item:selected {
            background: #F36F1A;
            color: white;
        }
    )").arg(arrowPath));
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPanelWidget::onLanguageChanged);

    // Auto-start row: label + checkbox
    m_autoStartLabel = new QLabel(tr("Launch at Login"), m_contentWidget);
    m_autoStartLabel->setFont(harmonyFont(10));
    m_autoStartLabel->setStyleSheet("color: black; background: transparent;");
    m_autoStartLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_autoStartCheck = new CheckMarkBox(m_contentWidget);
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
            background: #F36F1A;
            border: 1px solid #F36F1A;
        }
        QCheckBox::indicator:unchecked {
            background: white;
        }
    )");
    connect(m_autoStartCheck, &QCheckBox::toggled,
            this, &SettingsPanelWidget::onAutoStartToggled);

    // ECG row: label + checkbox
    m_ecgLabel = new QLabel(tr("ECG Monitor"), m_contentWidget);
    m_ecgLabel->setFont(harmonyFont(10));
    m_ecgLabel->setStyleSheet("color: black; background: transparent;");
    m_ecgLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_ecgCheck = new CheckMarkBox(m_contentWidget);
    m_ecgCheck->setFixedSize(16, 16);
    m_ecgCheck->setStyleSheet(R"(
        QCheckBox::indicator {
            width: 12px;
            height: 12px;
            background: white;
            border: 2px solid black;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background: #F36F1A;
            border: 1px solid #F36F1A;
        }
        QCheckBox::indicator:unchecked {
            background: white;
        }
    )");
    connect(m_ecgCheck, &QCheckBox::toggled,
            this, &SettingsPanelWidget::onEcgToggled);

    // Port row: label + input
    m_portLabel = new QLabel(tr("Port"), m_contentWidget);
    m_portLabel->setFont(harmonyFont(10));
    m_portLabel->setStyleSheet("color: black; background: transparent;");
    m_portLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_portInput = new QLineEdit(m_contentWidget);
    m_portInput->setFont(harmonyFont(10));
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

    // Pack selection row: label + cascading button (mirrors the tray Pet menu)
    m_packLabel = new QLabel(tr("Model"), m_contentWidget);
    m_packLabel->setFont(harmonyFont(10));
    m_packLabel->setStyleSheet("color: black; background: transparent;");
    m_packLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_packButton = new QToolButton(m_contentWidget);
    m_packButton->setFont(harmonyFont(10));
    m_packButton->setFixedHeight(24);
    m_packButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_packButton->setPopupMode(QToolButton::InstantPopup);
    m_packButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_packButton->setText(tr("(no pack)"));
    m_packButton->setCursor(Qt::PointingHandCursor);
    m_packButton->setStyleSheet(QStringLiteral(R"(
        QToolButton {
            background: white;
            border: 2px solid black;
            border-radius: 3px;
            padding: 2px 22px 2px 6px;   /* right pad for arrow */
            color: #2C2C2E;
            min-width: 70px;
            text-align: left;
        }
        QToolButton::menu-indicator {
            image: url(%1);
            subcontrol-origin: padding;
            subcontrol-position: center right;
            right: 6px;
            width: 8px;
            height: 5px;
        }
        QToolButton:hover {
            background: #F36F1A;
            color: white;
        }
        QMenu {
            background: white;
            border: 2px solid black;
            border-radius: 4px;
            color: #2C2C2E;
            padding: 2px;
        }
        QMenu::item {
            padding: 4px 18px 4px 10px;
        }
        QMenu::item:selected {
            background: #F36F1A;
            color: white;
        }
        QMenu::item:checked {
            font-weight: bold;
        }
        QMenu::separator {
            height: 1px;
            background: black;
            margin: 2px 4px;
        }
    )").arg(arrowPath));

    // Grid layout for form rows: labels in col 0, controls in col 1
    QGridLayout *formGrid = new QGridLayout();
    formGrid->setHorizontalSpacing(10);
    formGrid->setVerticalSpacing(VERTICAL_SPACING);
    formGrid->setColumnStretch(1, 1);  // controls column stretches

    formGrid->addWidget(m_langLabel,       0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_langCombo,       0, 1);
    formGrid->addWidget(m_autoStartLabel,  1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_autoStartCheck,  1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_ecgLabel,        2, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_ecgCheck,        2, 1, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_portLabel,       3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_portInput,       3, 1);
    formGrid->addWidget(m_packLabel,       4, 0, Qt::AlignLeft | Qt::AlignVCenter);
    formGrid->addWidget(m_packButton,      4, 1);

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
    hideAnimated();
}

void SettingsPanelWidget::setPanelScale(qreal s)
{
    m_scale = s;
    // Scale the content widget via transform from center
    QTransform t;
    qreal cx = SHADOW_BLUR + PANEL_WIDTH / 2.0;
    qreal cy = SHADOW_BLUR + PANEL_HEIGHT / 2.0;
    t.translate(cx, cy);
    t.scale(s, s);
    t.translate(-cx, -cy);
    m_contentWidget->setGeometry(
        SHADOW_BLUR + static_cast<int>(cx * (1.0 - s)),
        SHADOW_BLUR + static_cast<int>(cy * (1.0 - s)),
        static_cast<int>(PANEL_WIDTH * s),
        static_cast<int>(PANEL_HEIGHT * s));
    update();
}

void SettingsPanelWidget::setPanelOpacity(qreal o)
{
    m_panelOpacity = o;
    setWindowOpacity(o);
}

void SettingsPanelWidget::showAnimated()
{
    m_scale = 0.9;
    m_panelOpacity = 0.0;
    setWindowOpacity(0.0);
    QWidget::show();
    raise();

    delete m_scaleAnim;
    delete m_opacityAnim;

    // Scale: 0.9 → 1.0 with overshoot
    m_scaleAnim = new QPropertyAnimation(this, "panelScale", this);
    m_scaleAnim->setDuration(300);
    m_scaleAnim->setStartValue(0.9);
    m_scaleAnim->setEndValue(1.0);
    m_scaleAnim->setEasingCurve(QEasingCurve::OutBack);
    m_scaleAnim->start();

    // Fade in
    m_opacityAnim = new QPropertyAnimation(this, "panelOpacity", this);
    m_opacityAnim->setDuration(250);
    m_opacityAnim->setStartValue(0.0);
    m_opacityAnim->setEndValue(1.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_opacityAnim->start();
}

void SettingsPanelWidget::hideAnimated()
{
    delete m_scaleAnim;
    delete m_opacityAnim;

    // Scale: 1.0 → 0.9
    m_scaleAnim = new QPropertyAnimation(this, "panelScale", this);
    m_scaleAnim->setDuration(200);
    m_scaleAnim->setStartValue(1.0);
    m_scaleAnim->setEndValue(0.9);
    m_scaleAnim->setEasingCurve(QEasingCurve::InCubic);
    m_scaleAnim->start();

    // Fade out → hide when done
    m_opacityAnim = new QPropertyAnimation(this, "panelOpacity", this);
    m_opacityAnim->setDuration(200);
    m_opacityAnim->setStartValue(m_panelOpacity);
    m_opacityAnim->setEndValue(0.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::InCubic);
    connect(m_opacityAnim, &QPropertyAnimation::finished, this, &QWidget::hide);
    m_opacityAnim->start();
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

void SettingsPanelWidget::onEcgToggled(bool checked)
{
    m_config->setEcgEnabled(checked);
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
    if (m_packManager) {
        // Keep the button label in sync when the active pack changes via any
        // other path (system tray, hot reload, etc).
        connect(m_packManager, &CharacterPackManager::activePackChanged,
                this, [this]() { updatePackButtonLabel(); });
    }
    refreshPackList();
}

// Mirror SystemTray::kCategoryOrder so the two menu surfaces show the same
// grouping in the same order. Keeping this in lock-step with the tray.
static const struct {
    const char *id;
    const char *labelEn;
} kCategoryOrder[] = {
    { "originals",       QT_TR_NOOP("Standalone") },
    { "azur_lane",       QT_TR_NOOP("Azur Lane") },
    { "girls_frontline", QT_TR_NOOP("Girls' Frontline") },
    { "idol_dimension",  QT_TR_NOOP("Idol Dimension") },
    { "konosuba",        QT_TR_NOOP("Konosuba") },
    { "live2d_samples",  QT_TR_NOOP("Live2D Samples") },
};

void SettingsPanelWidget::refreshPackList()
{
    if (!m_packButton) {
        return;
    }

    if (QMenu *old = m_packButton->menu()) {
        m_packButton->setMenu(nullptr);
        old->deleteLater();
    }

    if (!m_packManager) {
        m_packButton->setText(tr("(no pack)"));
        return;
    }

    QMenu *menu = new QMenu(m_packButton);
    menu->setFont(m_packButton->font());

    const auto packs = m_packManager->availablePacks();
    const QString activeId = m_packManager->activePackId();
    const QString locale = m_packManager->activeLocale();

    // Group by category (matches SystemTray::refreshPackMenu).
    QMap<QString, QVector<CharacterPackManager::PackInfo>> grouped;
    for (const auto &pack : packs) {
        const QString cat = pack.category.isEmpty()
                                ? QStringLiteral("originals") : pack.category;
        grouped[cat].append(pack);
    }

    QActionGroup *group = new QActionGroup(menu);
    group->setExclusive(true);

    auto addToSubmenu = [&](QMenu *sub, const QVector<CharacterPackManager::PackInfo> &list) {
        for (const auto &pack : list) {
            QAction *action = sub->addAction(pack.displayName(locale));
            action->setCheckable(true);
            action->setChecked(pack.id == activeId);
            group->addAction(action);
            const QString packId = pack.id;
            connect(action, &QAction::triggered, this, [this, packId]() {
                if (m_packManager) m_packManager->switchPack(packId);
            });
        }
    };

    QSet<QString> seen;
    for (const auto &c : kCategoryOrder) {
        const QString id = QString::fromLatin1(c.id);
        if (!grouped.contains(id)) continue;
        QMenu *sub = menu->addMenu(tr(c.labelEn));
        sub->setFont(m_packButton->font());
        addToSubmenu(sub, grouped[id]);
        seen.insert(id);
    }
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        if (seen.contains(it.key())) continue;
        QMenu *sub = menu->addMenu(it.key());
        sub->setFont(m_packButton->font());
        addToSubmenu(sub, it.value());
    }

    m_packButton->setMenu(menu);
    updatePackButtonLabel();
}

void SettingsPanelWidget::updatePackButtonLabel()
{
    if (!m_packButton || !m_packManager) return;
    const QString activeId = m_packManager->activePackId();
    const QString locale = m_packManager->activeLocale();
    for (const auto &pack : m_packManager->availablePacks()) {
        if (pack.id == activeId) {
            m_packButton->setText(pack.displayName(locale));
            return;
        }
    }
    m_packButton->setText(tr("(no pack)"));
}

void SettingsPanelWidget::retranslateUi()
{
    m_titleLabel->setText(tr("Settings"));
    m_closeButton->setText(tr("×"));
    m_langLabel->setText(tr("Language"));
    m_langCombo->setItemText(0, tr("English"));
    m_langCombo->setItemText(1, tr("简体中文"));
    m_autoStartLabel->setText(tr("Launch at Login"));
    m_ecgLabel->setText(tr("ECG Monitor"));
    m_portLabel->setText(tr("Port"));
    m_packLabel->setText(tr("Model"));
    // Pack labels can switch between English/Chinese on locale change.
    if (m_packManager) {
        refreshPackList();
    }
}
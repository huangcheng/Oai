#include "PackManagerDialog.h"
#include "CharacterPackManager.h"
#include "StyledAlertDialog.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFileDialog>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>
#include <QStyle>
#include <QStandardPaths>

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

static QFont harmonyFont(int pointSize, QFont::Weight weight = QFont::Normal)
{
    QFont f(QStringLiteral("HarmonyOS Sans SC"), pointSize, weight);
    f.setStyleStrategy(QFont::PreferAntialias);
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

PackManagerDialog::PackManagerDialog(CharacterPackManager *manager, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_packManager(manager)
{
    setWindowFlags(
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::WindowDoesNotAcceptFocus
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif

    setFixedSize(PANEL_WIDTH + SHADOW_BLUR * 2, PANEL_HEIGHT + SHADOW_BLUR * 2);

    setupUi();

    if (m_packManager) {
        connect(m_packManager, &CharacterPackManager::packListChanged,
                this, &PackManagerDialog::refreshPackList);
    }
    refreshPackList();
}

void PackManagerDialog::setupUi()
{
    m_contentWidget = new QWidget(this);
    m_contentWidget->setGeometry(SHADOW_BLUR, SHADOW_BLUR, PANEL_WIDTH, PANEL_HEIGHT);
    m_contentWidget->setStyleSheet("background: transparent;");

    QVBoxLayout *mainLayout = new QVBoxLayout(m_contentWidget);
    mainLayout->setContentsMargins(PADDING, PADDING, PADDING, PADDING);
    mainLayout->setSpacing(VERTICAL_SPACING);

    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(4);

    m_titleLabel = new QLabel(tr("Models Management"), m_contentWidget);
    m_titleLabel->setFont(harmonyFont(10, QFont::Bold));
    m_titleLabel->setStyleSheet("color: black; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_closeButton = new QPushButton(tr("\u00D7"), m_contentWidget);
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
    connect(m_closeButton, &QPushButton::clicked, this, &PackManagerDialog::onCloseClicked);

    titleRow->addWidget(m_titleLabel, 1);
    titleRow->addWidget(m_closeButton);

    m_listWidget = new QListWidget(m_contentWidget);
    m_listWidget->setFont(harmonyFont(10));
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listWidget->setStyleSheet(R"(
        QListWidget {
            background: white;
            border: 2px solid black;
            border-radius: 3px;
            color: #2C2C2E;
            padding: 4px;
            outline: none;
        }
        QListWidget::item {
            padding: 6px 8px;
            border-radius: 2px;
            color: #2C2C2E;
        }
        QListWidget::item:selected {
            background: #F36F1A;
            color: white;
        }
        QListWidget::item:hover {
            background: #FFF0E6;
        }
        QListWidget::item:selected:hover {
            background: #F36F1A;
            color: white;
        }
        QScrollBar:vertical {
            background: #f5f5f5;
            width: 8px;
            border-radius: 4px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #888;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover {
            background: #F36F1A;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: none;
        }
    )");

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);

    m_addButton = new QPushButton(tr("Add"), m_contentWidget);
    m_addButton->setFont(harmonyFont(10));
    m_addButton->setFixedHeight(28);
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setStyleSheet(R"(
        QPushButton {
            background: white;
            border: 2px solid black;
            border-radius: 3px;
            padding: 2px 16px;
            color: #2C2C2E;
            min-width: 60px;
            outline: none;
        }
        QPushButton:hover {
            background: #F36F1A;
            color: white;
            border-color: #F36F1A;
        }
        QPushButton:pressed {
            background: #E06516;
            border-color: #E06516;
        }
    )");
    connect(m_addButton, &QPushButton::clicked, this, &PackManagerDialog::onAddClicked);

    m_deleteButton = new QPushButton(tr("Delete"), m_contentWidget);
    m_deleteButton->setFont(harmonyFont(10));
    m_deleteButton->setFixedHeight(28);
    m_deleteButton->setCursor(Qt::PointingHandCursor);
    m_deleteButton->setStyleSheet(R"(
        QPushButton {
            background: white;
            border: 2px solid black;
            border-radius: 3px;
            padding: 2px 16px;
            color: #2C2C2E;
            min-width: 60px;
            outline: none;
        }
        QPushButton:hover {
            background: #F36F1A;
            color: white;
            border-color: #F36F1A;
        }
        QPushButton:pressed {
            background: #E06516;
            border-color: #E06516;
        }
        QPushButton:disabled {
            background: #f0f0f0;
            color: #999;
            border-color: #ccc;
        }
    )");
    connect(m_deleteButton, &QPushButton::clicked, this, &PackManagerDialog::onDeleteClicked);

    buttonRow->addStretch(1);
    buttonRow->addWidget(m_addButton);
    buttonRow->addWidget(m_deleteButton);

    mainLayout->addLayout(titleRow);
    mainLayout->addWidget(m_listWidget, 1);
    mainLayout->addLayout(buttonRow);
}

void PackManagerDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body(SHADOW_BLUR, SHADOW_BLUR, PANEL_WIDTH, PANEL_HEIGHT);
    const qreal r = CORNER_RADIUS;
    const qreal sk = SKEW_PX;

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

    // Shadow
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

    // Orange accent stripe at top
    painter.save();
    painter.setClipPath(panelPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xF3, 0x6F, 0x1A));
    painter.drawRect(QRectF(body.left(), body.top(), body.width() + sk, 4));
    painter.restore();
}

void PackManagerDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        const int doNotRound = 1;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &doNotRound, sizeof(doNotRound));
        const int backdropNone = 1;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                              &backdropNone, sizeof(backdropNone));
        const int ncRenderingDisabled = 1;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                              &ncRenderingDisabled, sizeof(ncRenderingDisabled));
    }
#endif
    positionCentered();
}

void PackManagerDialog::positionCentered()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect screenRect = screen->availableGeometry();
    int x = screenRect.center().x() - (PANEL_WIDTH + SHADOW_BLUR * 2) / 2;
    int y = screenRect.center().y() - (PANEL_HEIGHT + SHADOW_BLUR * 2) / 2;

    move(x, y);
}

void PackManagerDialog::showAnimated()
{
    m_scale = 0.9;
    m_panelOpacity = 0.0;
    setWindowOpacity(0.0);
    QWidget::show();
    raise();

    delete m_scaleAnim;
    delete m_opacityAnim;

    m_scaleAnim = new QPropertyAnimation(this, "panelScale", this);
    m_scaleAnim->setDuration(300);
    m_scaleAnim->setStartValue(0.9);
    m_scaleAnim->setEndValue(1.0);
    m_scaleAnim->setEasingCurve(QEasingCurve::OutBack);
    m_scaleAnim->start();

    m_opacityAnim = new QPropertyAnimation(this, "panelOpacity", this);
    m_opacityAnim->setDuration(250);
    m_opacityAnim->setStartValue(0.0);
    m_opacityAnim->setEndValue(1.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_opacityAnim->start();
}

void PackManagerDialog::hideAnimated()
{
    delete m_scaleAnim;
    delete m_opacityAnim;

    m_scaleAnim = new QPropertyAnimation(this, "panelScale", this);
    m_scaleAnim->setDuration(200);
    m_scaleAnim->setStartValue(1.0);
    m_scaleAnim->setEndValue(0.9);
    m_scaleAnim->setEasingCurve(QEasingCurve::InCubic);
    m_scaleAnim->start();

    m_opacityAnim = new QPropertyAnimation(this, "panelOpacity", this);
    m_opacityAnim->setDuration(200);
    m_opacityAnim->setStartValue(m_panelOpacity);
    m_opacityAnim->setEndValue(0.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::InCubic);
    connect(m_opacityAnim, &QPropertyAnimation::finished, this, &QWidget::hide);
    m_opacityAnim->start();
}

void PackManagerDialog::setPanelScale(qreal s)
{
    m_scale = s;
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

void PackManagerDialog::setPanelOpacity(qreal o)
{
    m_panelOpacity = o;
    setWindowOpacity(o);
}

void PackManagerDialog::onCloseClicked()
{
    hideAnimated();
}

void PackManagerDialog::onAddClicked()
{
    if (!m_packManager) return;

    QStringList filters;
    filters << tr("Pack files (*.opk *.codex-pet)")
            << tr("OPK files (*.opk)")
            << tr("Codex Pet files (*.codex-pet)")
            << tr("All files (*)");

    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select Pack Files to Install"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        filters.join(";;")
    );

    if (files.isEmpty()) return;

    int successCount = 0;
    int failCount = 0;
    QStringList failedFiles;

    for (const QString &file : files) {
        if (m_packManager->installPack(file)) {
            ++successCount;
        } else {
            ++failCount;
            failedFiles.append(QFileInfo(file).fileName());
        }
    }

    if (successCount > 0) {
        QString msg = tr("Successfully installed %1 pack(s).").arg(successCount);
        if (failCount > 0) {
            msg += QLatin1Char('\n') + tr("Failed to install %1 file(s): %2")
                     .arg(failCount).arg(failedFiles.join(", "));
        }
        if (!m_alertDialog) {
            m_alertDialog = new StyledAlertDialog(nullptr);
        }
        m_alertDialog->showAlert(tr("Installation Complete"), msg);
    } else if (failCount > 0) {
        if (!m_alertDialog) {
            m_alertDialog = new StyledAlertDialog(nullptr);
        }
        m_alertDialog->showAlert(tr("Installation Failed"),
            tr("Failed to install all selected files: %1")
                .arg(failedFiles.join(", ")));
    }
}

void PackManagerDialog::onDeleteClicked()
{
    if (!m_packManager) return;

    QList<QListWidgetItem *> selected = m_listWidget->selectedItems();
    if (selected.isEmpty()) {
        if (!m_alertDialog) {
            m_alertDialog = new StyledAlertDialog(nullptr);
        }
        m_alertDialog->showAlert(tr("No Selection"),
            tr("Please select one or more packs to delete."));
        return;
    }

    QStringList packNames;
    QStringList packIds;
    for (QListWidgetItem *item : selected) {
        packIds.append(item->data(Qt::UserRole).toString());
        packNames.append(item->text());
    }

    QString activePackId = m_packManager->activePackId();
    bool hasActivePack = packIds.contains(activePackId);

    if (hasActivePack) {
        if (!m_alertDialog) {
            m_alertDialog = new StyledAlertDialog(nullptr);
        }
        CharacterPackManager::PackInfo activeInfo = m_packManager->packInfo(activePackId);
        QString activeName = activeInfo.displayName(m_packManager->activeLocale());
        m_alertDialog->showAlert(
            tr("Cannot Delete Active Pet"),
            tr("\"%1\" is currently in use and cannot be deleted.\n\nPlease switch to another pet first.")
                .arg(activeName)
        );
        return;
    }

    StyledAlertDialog confirmDialog(nullptr);
    bool confirmed = confirmDialog.execConfirm(
        tr("Delete Packs"),
        tr("Are you sure you want to delete the following %1 pack(s)?\n\n%2\n\nThis action cannot be undone.")
            .arg(packIds.size())
            .arg(packNames.join("\n"))
    );

    if (!confirmed) return;

    int successCount = 0;
    int failCount = 0;
    QStringList failedNames;

    for (const QString &packId : packIds) {
        CharacterPackManager::PackInfo info = m_packManager->packInfo(packId);
        if (m_packManager->uninstallPack(packId)) {
            ++successCount;
        } else {
            ++failCount;
            failedNames.append(info.displayName(m_packManager->activeLocale()));
        }
    }

    if (successCount > 0 && failCount == 0) {
        if (!m_alertDialog) {
            m_alertDialog = new StyledAlertDialog(nullptr);
        }
        m_alertDialog->showAlert(tr("Delete Complete"),
            tr("Successfully deleted %1 pack(s).").arg(successCount));
    } else if (failCount > 0) {
        QString msg = tr("Successfully deleted %1 pack(s).").arg(successCount);
        msg += QLatin1Char('\n') + tr("Failed to delete: %1").arg(failedNames.join(", "));
        if (!m_alertDialog) {
            m_alertDialog = new StyledAlertDialog(nullptr);
        }
        m_alertDialog->showAlert(tr("Delete Partial"), msg);
    }
}

void PackManagerDialog::refreshPackList()
{
    if (!m_listWidget || !m_packManager) return;

    m_listWidget->clear();

    const auto packs = m_packManager->availablePacks();
    const QString locale = m_packManager->activeLocale();

    for (const auto &pack : packs) {
        if (pack.source != CharacterPackManager::PackSource::User) {
            qDebug() << "PackManagerDialog: Filtering out built-in pack:" << pack.id << pack.name;
            continue;
        }

        QListWidgetItem *item = new QListWidgetItem(pack.displayName(locale));
        item->setData(Qt::UserRole, pack.id);
        item->setToolTip(tr("ID: %1\nPath: %2").arg(pack.id, pack.path));
        m_listWidget->addItem(item);
        qDebug() << "PackManagerDialog: Showing user pack:" << pack.id << pack.name;
    }

    // Update delete button state
    m_deleteButton->setEnabled(m_listWidget->count() > 0);
}

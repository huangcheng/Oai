#include "StyledAlertDialog.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>

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

StyledAlertDialog::StyledAlertDialog(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowFlags(
        Qt::Dialog |
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
}

void StyledAlertDialog::setupUi()
{
    m_contentWidget = new QWidget(this);
    m_contentWidget->setGeometry(SHADOW_BLUR, SHADOW_BLUR, PANEL_WIDTH, PANEL_HEIGHT);
    m_contentWidget->setStyleSheet("background: transparent;");

    QVBoxLayout *mainLayout = new QVBoxLayout(m_contentWidget);
    mainLayout->setContentsMargins(PADDING, PADDING, PADDING, PADDING);
    mainLayout->setSpacing(VERTICAL_SPACING);

    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(4);

    m_titleLabel = new QLabel(m_contentWidget);
    m_titleLabel->setFont(harmonyFont(10, QFont::Bold));
    m_titleLabel->setStyleSheet("color: black; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setWordWrap(true);

    m_closeButton = new QPushButton(QStringLiteral("\u00D7"), m_contentWidget);
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
    connect(m_closeButton, &QPushButton::clicked, this, &StyledAlertDialog::onCloseClicked);

    titleRow->addWidget(m_titleLabel, 1);
    titleRow->addWidget(m_closeButton);

    m_bodyLabel = new QLabel(m_contentWidget);
    m_bodyLabel->setFont(harmonyFont(10));
    m_bodyLabel->setStyleSheet("color: #2C2C2E; background: transparent;");
    m_bodyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_bodyLabel->setWordWrap(true);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);

    m_okButton = new QPushButton(tr("OK"), m_contentWidget);
    m_okButton->setFont(harmonyFont(10));
    m_okButton->setFixedHeight(28);
    m_okButton->setCursor(Qt::PointingHandCursor);
    m_okButton->setStyleSheet(R"(
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
    connect(m_okButton, &QPushButton::clicked, this, &StyledAlertDialog::onOkClicked);

    buttonRow->addStretch(1);
    buttonRow->addWidget(m_okButton);

    mainLayout->addLayout(titleRow);
    mainLayout->addWidget(m_bodyLabel, 1);
    mainLayout->addLayout(buttonRow);
}

void StyledAlertDialog::showAlert(const QString &title, const QString &body,
                                   const QString &buttonText)
{
    m_titleLabel->setText(title);
    m_bodyLabel->setText(body);
    if (!buttonText.isEmpty()) {
        m_okButton->setText(buttonText);
    } else {
        m_okButton->setText(tr("OK"));
    }

    showAnimated();
}

void StyledAlertDialog::showAnimated()
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

void StyledAlertDialog::hideAnimated()
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
    connect(m_opacityAnim, &QPropertyAnimation::finished, this, [this]() {
        QWidget::hide();
        emit dismissed();
    });
    m_opacityAnim->start();
}

void StyledAlertDialog::setPanelScale(qreal s)
{
    m_scale = s;
    qreal cx = SHADOW_BLUR + PANEL_WIDTH / 2.0;
    qreal cy = SHADOW_BLUR + PANEL_HEIGHT / 2.0;
    m_contentWidget->setGeometry(
        SHADOW_BLUR + static_cast<int>(cx * (1.0 - s)),
        SHADOW_BLUR + static_cast<int>(cy * (1.0 - s)),
        static_cast<int>(PANEL_WIDTH * s),
        static_cast<int>(PANEL_HEIGHT * s));
    update();
}

void StyledAlertDialog::setPanelOpacity(qreal o)
{
    m_panelOpacity = o;
    setWindowOpacity(o);
}

void StyledAlertDialog::paintEvent(QPaintEvent *event)
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

    painter.save();
    painter.setOpacity(0.35);
    painter.setPen(Qt::NoPen);
    QPainterPath shadowPath = panelPath;
    shadowPath.translate(3, 4);
    painter.setBrush(Qt::black);
    painter.drawPath(shadowPath);
    painter.restore();

    painter.setPen(QPen(Qt::black, BORDER_WIDTH, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(Qt::white);
    painter.drawPath(panelPath);

    painter.save();
    painter.setClipPath(panelPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xF3, 0x6F, 0x1A));
    painter.drawRect(QRectF(body.left(), body.top(), body.width() + sk, ACCENT_HEIGHT));
    painter.restore();
}

void StyledAlertDialog::showEvent(QShowEvent *event)
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

void StyledAlertDialog::positionCentered()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect screenRect = screen->availableGeometry();
    int x = screenRect.center().x() - (PANEL_WIDTH + SHADOW_BLUR * 2) / 2;
    int y = screenRect.center().y() - (PANEL_HEIGHT + SHADOW_BLUR * 2) / 2;

    move(x, y);
}

void StyledAlertDialog::onOkClicked()
{
    hideAnimated();
}

void StyledAlertDialog::onCloseClicked()
{
    hideAnimated();
}

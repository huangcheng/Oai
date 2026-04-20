#include "TipBubbleWidget.h"

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QPaintEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QRect>
#include <QPainterPath>

TipBubbleWidget::TipBubbleWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool |
        Qt::WindowDoesNotAcceptFocus
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    m_opacity = 1.0;

    // Connect dismiss timer
    connect(&m_dismissTimer, &QTimer::timeout, this, &TipBubbleWidget::hideBubble);
}

TipBubbleWidget::~TipBubbleWidget()
{
    if (m_opacityAnim) {
        m_opacityAnim->deleteLater();
    }
}

void TipBubbleWidget::anchorTo(const QWidget *petWidget)
{
    m_anchoredPet = petWidget;
    if (petWidget && isVisible()) {
        positionRelativeTo(petWidget);
    }
}

void TipBubbleWidget::showBubble(const QString &title, const QString &message, BubbleType type)
{
    m_title = title;
    m_message = message;
    m_type = type;

    // Calculate text layout and bubble dimensions
    calculateTextLayout();

    // Set widget size
    int totalWidth = m_bubbleRect.width();
    int totalHeight = m_bubbleRect.height() + TAIL_HEIGHT;
    setFixedSize(totalWidth, totalHeight);

    // Position relative to pet
    if (m_anchoredPet) {
        positionRelativeTo(m_anchoredPet);
    } else {
        // Default position if no pet anchored
        QRect screenRect = QGuiApplication::primaryScreen()->availableGeometry();
        move(screenRect.center().x() - width() / 2,
             screenRect.center().y() - height() / 2);
    }

    // Update bubble path for painting
    updateBubblePath();

    // Reset and start enter animation
    m_opacity = 0.0;
    startEnterAnimation();

    // Show widget
    QWidget::show();

    // Start dismiss timer
    m_dismissTimer.stop();
    int dismissMs = (type == StatusBubble) ? STATUS_DISMISS_MS : TIP_DISMISS_MS;
    m_dismissTimer.start(dismissMs);
}

void TipBubbleWidget::hideBubble()
{
    m_dismissTimer.stop();
    startExitAnimation();
}

void TipBubbleWidget::setOpacity(qreal o)
{
    m_opacity = o;
    update();
}

void TipBubbleWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    // NO antialiasing — Win98 was pixel-crisp
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Asymmetric tail: right edge nearly vertical, left edge sharply sloped
    // This makes the tail "point" diagonally toward Clippy like a real speech bubble
    int tailRightX = m_bubbleRect.right() - TAIL_OFFSET_FROM_RIGHT;
    int tailTipX = tailRightX + 2;  // tip just slightly right of the vertical edge
    int tailLeftX = tailRightX - TAIL_WIDTH; // left edge is far left, creating the slope

    // Build a single continuous path: rounded rect body + tail triangle
    const int r = CORNER_RADIUS;
    const QRect &b = m_bubbleRect;

    QPainterPath path;
    if (m_tailDown) {
        // Start top-left, go clockwise
        path.moveTo(b.left() + r, b.top());
        path.lineTo(b.right() - r, b.top());
        path.quadTo(b.right(), b.top(), b.right(), b.top() + r);
        path.lineTo(b.right(), b.bottom() - r);
        path.quadTo(b.right(), b.bottom(), b.right() - r, b.bottom());
        // Bottom edge: run right-to-left along the bottom
        // At tailRightX, the nearly-vertical right edge drops to the tip
        path.lineTo(tailRightX, b.bottom());
        path.lineTo(tailTipX, b.bottom() + TAIL_HEIGHT); // tip: slightly right, mostly down
        path.lineTo(tailLeftX, b.bottom());               // left base: far left = steep slope
        // Continue bottom edge to left corner
        path.lineTo(b.left() + r, b.bottom());
        path.quadTo(b.left(), b.bottom(), b.left(), b.bottom() - r);
        path.lineTo(b.left(), b.top() + r);
        path.quadTo(b.left(), b.top(), b.left() + r, b.top());
    } else {
        // Tail points up: mirror of above
        path.moveTo(b.left() + r, b.bottom());
        path.lineTo(b.right() - r, b.bottom());
        path.quadTo(b.right(), b.bottom(), b.right(), b.bottom() - r);
        path.lineTo(b.right(), b.top() + r);
        path.quadTo(b.right(), b.top(), b.right() - r, b.top());
        // Top edge: right-to-left
        path.lineTo(tailRightX, b.top());
        path.lineTo(tailTipX, b.top() - TAIL_HEIGHT);
        path.lineTo(tailLeftX, b.top());
        path.lineTo(b.left() + r, b.top());
        path.quadTo(b.left(), b.top(), b.left(), b.top() + r);
        path.lineTo(b.left(), b.bottom() - r);
        path.quadTo(b.left(), b.bottom(), b.left() + r, b.bottom());
    }
    path.closeSubpath();

    // Draw hard shadow (solid black, no blur, offset by SHADOW_OFFSET)
    painter.save();
    painter.setOpacity(m_opacity);
    QPainterPath shadowPath = path;
    shadowPath.translate(SHADOW_OFFSET, SHADOW_OFFSET);
    painter.fillPath(shadowPath, Qt::black);
    painter.restore();

    // Draw bubble background (#FFFFE1)
    painter.save();
    painter.setOpacity(m_opacity);
    painter.fillPath(path, QColor(255, 255, 225));
    painter.strokePath(path, QPen(Qt::black, 1));
    painter.restore();

    // Draw title text (bold)
    if (!m_title.isEmpty()) {
        painter.save();
        painter.setOpacity(m_opacity);
        QFont titleFont("Tahoma", 9, QFont::Bold);
        painter.setFont(titleFont);
        painter.setPen(Qt::black);
        painter.drawText(m_titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);
        painter.restore();
    }

    // Draw message text (normal weight)
    if (!m_message.isEmpty()) {
        painter.save();
        painter.setOpacity(m_opacity);
        QFont msgFont("Tahoma", 9);
        painter.setFont(msgFont);
        painter.setPen(Qt::black);

        QFontMetrics fm(msgFont);
        QString wrappedText = fm.elidedText(m_message, Qt::ElideRight, m_messageRect.width());
        painter.drawText(m_messageRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, wrappedText);
        painter.restore();
    }
}

void TipBubbleWidget::positionRelativeTo(const QWidget *pet)
{
    if (!pet)
        return;

    QPoint petCenter = pet->mapToGlobal(QPoint(pet->width() / 2, pet->height() / 2));
    int petTop = pet->mapToGlobal(QPoint(0, 0)).y();
    int petBottom = pet->mapToGlobal(QPoint(0, pet->height())).y();
    int petCenterX = petCenter.x();

    // Calculate bubble position: align tail tip with pet center, not bubble center
    // Tail tip X in widget coords = m_bubbleRect.right() - TAIL_OFFSET_FROM_RIGHT + 2
    int tailTipLocalX = m_bubbleRect.right() - TAIL_OFFSET_FROM_RIGHT + 2;
    int bubbleX = petCenterX - tailTipLocalX;
    int bubbleY = petTop - m_bubbleRect.height() - TAIL_HEIGHT;

    m_tailDown = true; // default: tail points down

    // Check if not enough room above
    QScreen *screen = QGuiApplication::screenAt(petCenter);
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        if (bubbleY < screenRect.top()) {
            // Not enough room above — place below pet
            bubbleY = petBottom + TAIL_HEIGHT;
            m_tailDown = false;
        }
    }

    // Update tail polygon for new position
    updateBubblePath();

    // Clamp to screen boundaries
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        bubbleX = qBound(screenRect.left(), bubbleX, screenRect.right() - width());
        bubbleY = qBound(screenRect.top(), bubbleY, screenRect.bottom() - height());
    }

    move(bubbleX, bubbleY);
}

void TipBubbleWidget::startEnterAnimation()
{
    if (m_opacityAnim) {
        m_opacityAnim->deleteLater();
    }

    m_opacityAnim = new QPropertyAnimation(this, "opacity", this);
    m_opacityAnim->setDuration(200);
    m_opacityAnim->setStartValue(0.0);
    m_opacityAnim->setEndValue(1.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::OutQuad);
    m_opacityAnim->start();
}

void TipBubbleWidget::startExitAnimation()
{
    if (m_opacityAnim) {
        m_opacityAnim->deleteLater();
    }

    m_opacityAnim = new QPropertyAnimation(this, "opacity", this);
    m_opacityAnim->setDuration(150);
    m_opacityAnim->setStartValue(m_opacity);
    m_opacityAnim->setEndValue(0.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::InQuad);

    connect(m_opacityAnim, &QPropertyAnimation::finished, this, &QWidget::hide);
    m_opacityAnim->start();
}

void TipBubbleWidget::calculateTextLayout()
{
    QFont titleFont("Tahoma", 9, QFont::Bold);
    QFont msgFont("Tahoma", 9);

    QFontMetrics titleFm(titleFont);
    QFontMetrics msgFm(msgFont);

    // Calculate title height
    int titleHeight = m_title.isEmpty() ? 0 : titleFm.height();

    // Calculate message width with word wrap
    int textWidth = MAX_BUBBLE_WIDTH - (PADDING_H * 2);
    QString wrappedMsg = msgFm.elidedText(m_message, Qt::ElideRight, textWidth);

    // Calculate message bounding rect
    QRect msgBounding = msgFm.boundingRect(0, 0, textWidth, 1000,
                                            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                            wrappedMsg);
    int msgHeight = msgBounding.height();

    // Calculate total bubble dimensions
    int bubbleWidth = qMin(MAX_BUBBLE_WIDTH, qMax(titleFm.horizontalAdvance(m_title), msgFm.horizontalAdvance(wrappedMsg)) + PADDING_H * 2);
    int bubbleHeight = titleHeight + msgHeight + PADDING_V * 2;

    // Ensure minimum height
    if (!m_title.isEmpty() && msgHeight > 0) {
        bubbleHeight += 2; // Small spacing between title and message
    }

    m_bubbleRect.setWidth(bubbleWidth);
    m_bubbleRect.setHeight(bubbleHeight);

    // Calculate title rect
    int currentY = PADDING_V;
    if (!m_title.isEmpty()) {
        m_titleRect = QRect(PADDING_H, currentY,
                             bubbleWidth - PADDING_H * 2, titleHeight);
        currentY += titleHeight;
    } else {
        m_titleRect = QRect();
    }

    // Calculate message rect
    if (!m_message.isEmpty()) {
        m_messageRect = QRect(PADDING_H, currentY,
                              bubbleWidth - PADDING_H * 2, msgHeight);
    } else {
        m_messageRect = QRect();
    }
}

void TipBubbleWidget::updateBubblePath()
{
    // Asymmetric tail: right edge nearly vertical, left edge sloped
    int tailRightX = m_bubbleRect.right() - TAIL_OFFSET_FROM_RIGHT;
    int tailTipX = tailRightX + 2;
    int tailLeftX = tailRightX - TAIL_WIDTH;

    if (m_tailDown) {
        m_tailPoly.clear();
        m_tailPoly << QPoint(tailLeftX, m_bubbleRect.bottom())
                   << QPoint(tailTipX, m_bubbleRect.bottom() + TAIL_HEIGHT)
                   << QPoint(tailRightX, m_bubbleRect.bottom());
    } else {
        m_tailPoly.clear();
        m_tailPoly << QPoint(tailLeftX, m_bubbleRect.top())
                   << QPoint(tailTipX, m_bubbleRect.top() - TAIL_HEIGHT)
                   << QPoint(tailRightX, m_bubbleRect.top());
    }
}
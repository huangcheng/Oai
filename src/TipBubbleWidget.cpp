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
#include <QWindow>

TipBubbleWidget::TipBubbleWidget(QWidget *parent)
    : QWidget(parent)
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

void TipBubbleWidget::showBubble(const QString &title, const QString &message, BubbleType type, const QString &source)
{
    bool alreadyVisible = isVisible() && m_opacity > 0.5;
    bool sameContent = (m_title == title && m_message == message && m_source == source);

    m_title = title;
    m_message = message;
    m_source = source;
    m_type = type;

    // If already showing the same content, just reset the dismiss timer
    if (alreadyVisible && sameContent) {
        m_dismissTimer.stop();
        int dismissMs = (type == StatusBubble) ? STATUS_DISMISS_MS : TIP_DISMISS_MS;
        m_dismissTimer.start(dismissMs);
        return;
    }

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

    if (alreadyVisible) {
        // Content changed while visible — update text without re-fading
        update();
    } else {
        // First show — fade in
        m_opacity = 0.0;
        startEnterAnimation();
        QWidget::show();
    }

    // Reset dismiss timer
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
    // This makes the tail "point" diagonally toward Qlippy like a real speech bubble
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
        QFont titleFont("HarmonyOS Sans SC", 11, QFont::Bold);
        painter.setFont(titleFont);
        painter.setPen(Qt::black);
        painter.drawText(m_titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);
        painter.restore();
    }

    // Draw message text (normal weight)
    if (!m_message.isEmpty()) {
        painter.save();
        painter.setOpacity(m_opacity);
        QFont msgFont("HarmonyOS Sans SC", 11);
        painter.setFont(msgFont);
        painter.setPen(Qt::black);

        QFontMetrics fm(msgFont);
        QString wrappedText = fm.elidedText(m_message, Qt::ElideRight, m_messageRect.width());
        painter.drawText(m_messageRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, wrappedText);
        painter.restore();
    }

    // Draw source label (small gray text)
    if (!m_source.isEmpty()) {
        painter.save();
        painter.setOpacity(m_opacity);
        QFont sourceFont("HarmonyOS Sans SC", 9);
        painter.setFont(sourceFont);
        painter.setPen(QColor(120, 120, 120));
        painter.drawText(m_sourceRect, Qt::AlignLeft | Qt::AlignVCenter, m_source);
        painter.restore();
    }
}

void TipBubbleWidget::positionRelativeTo(const QWidget *pet)
{
    if (!pet)
        return;

    QRect anchor = m_anchorRect.isValid() ? m_anchorRect : QRect(0, 0, pet->width(), pet->height());

    // On macOS with Qt::Tool frameless windows, both mapToGlobal() and pos()
    // can return stale/incorrect coordinates. Use the native QWindow position
    // which tracks the actual OS window frame.
    QPoint petGlobalPos;
    if (QWindow *w = pet->windowHandle()) {
        petGlobalPos = w->position();
    } else {
        petGlobalPos = pet->pos();
    }
    QPoint petCenter(petGlobalPos.x() + anchor.x() + anchor.width() / 2,
                     petGlobalPos.y() + anchor.y() + anchor.height() / 2);
    int petCenterX = petCenter.x();
    int petTop      = petGlobalPos.y() + anchor.y();
    int petBottom   = petGlobalPos.y() + anchor.y() + anchor.height();

    // Calculate bubble position: align tail tip with pet center, not bubble center
    // Tail tip X in widget coords = m_bubbleRect.right() - TAIL_OFFSET_FROM_RIGHT + 2
    int tailTipLocalX = m_bubbleRect.right() - TAIL_OFFSET_FROM_RIGHT + 2;
    int bubbleX = petCenterX - tailTipLocalX;

    // Overlap the pet slightly so the tail feels attached, not floating
    constexpr int PET_OVERLAP = 6;
    int bubbleY = petTop - m_bubbleRect.height() - TAIL_HEIGHT + PET_OVERLAP;

    m_tailDown = true; // default: tail points down

    // Check if not enough room above
    QScreen *screen = QGuiApplication::screenAt(petCenter);
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        if (bubbleY < screenRect.top()) {
            // Not enough room above — place below pet with same overlap
            bubbleY = petBottom + TAIL_HEIGHT - PET_OVERLAP;
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
    m_opacityAnim->setDuration(400);
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
    m_opacityAnim->setDuration(300);
    m_opacityAnim->setStartValue(m_opacity);
    m_opacityAnim->setEndValue(0.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::InQuad);

    connect(m_opacityAnim, &QPropertyAnimation::finished, this, &QWidget::hide);
    m_opacityAnim->start();
}

void TipBubbleWidget::calculateTextLayout()
{
    QFont titleFont("HarmonyOS Sans SC", 11, QFont::Bold);
    QFont msgFont("HarmonyOS Sans SC", 11);
    QFont sourceFont("HarmonyOS Sans SC", 9);

    QFontMetrics titleFm(titleFont);
    QFontMetrics msgFm(msgFont);
    QFontMetrics sourceFm(sourceFont);

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

    // Calculate source label height
    int sourceHeight = m_source.isEmpty() ? 0 : sourceFm.height();

    // Calculate total bubble dimensions
    int contentWidth = qMax(titleFm.horizontalAdvance(m_title), msgFm.horizontalAdvance(wrappedMsg));
    if (!m_source.isEmpty()) {
        contentWidth = qMax(contentWidth, sourceFm.horizontalAdvance(m_source));
    }
    int bubbleWidth = qMin(MAX_BUBBLE_WIDTH, contentWidth + PADDING_H * 2);
    int bubbleHeight = titleHeight + msgHeight + sourceHeight + PADDING_V * 2;

    // Ensure minimum height
    if (!m_title.isEmpty() && msgHeight > 0) {
        bubbleHeight += 2; // Small spacing between title and message
    }
    if (sourceHeight > 0) {
        bubbleHeight += 2; // Small spacing before source label
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
        currentY += msgHeight;
    } else {
        m_messageRect = QRect();
    }

    // Calculate source rect
    if (!m_source.isEmpty()) {
        currentY += 2; // spacing
        m_sourceRect = QRect(PADDING_H, currentY,
                             bubbleWidth - PADDING_H * 2, sourceHeight);
    } else {
        m_sourceRect = QRect();
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
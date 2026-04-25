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

    // Set widget size: bubble + tail + shadow margin on all sides
    int totalWidth = m_bubbleRect.width() + SHADOW_BLUR * 2;
    int totalHeight = m_bubbleRect.height() + TAIL_HEIGHT + SHADOW_BLUR * 2;
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
        raise();
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
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Bubble body rect offset by shadow margin
    const QRectF body(SHADOW_BLUR, SHADOW_BLUR,
                      m_bubbleRect.width(), m_bubbleRect.height());
    const qreal r = CORNER_RADIUS;
    const qreal sk = SKEW_PX;  // parallelogram skew

    // Build skewed bubble path (parallelogram with slight rounded corners)
    // Top edge shifted right by sk, bottom edge stays — gives a dynamic lean
    QPainterPath bubblePath;
    bubblePath.moveTo(body.left() + sk + r, body.top());
    bubblePath.lineTo(body.right() + sk - r, body.top());
    bubblePath.quadTo(body.right() + sk, body.top(), body.right() + sk, body.top() + r);
    bubblePath.lineTo(body.right(), body.bottom() - r);
    bubblePath.quadTo(body.right(), body.bottom(), body.right() - r, body.bottom());
    bubblePath.lineTo(body.left() + r, body.bottom());
    bubblePath.quadTo(body.left(), body.bottom(), body.left(), body.bottom() - r);
    bubblePath.lineTo(body.left() + sk, body.top() + r);
    bubblePath.quadTo(body.left() + sk, body.top(), body.left() + sk + r, body.top());
    bubblePath.closeSubpath();

    // Build tail: angular, offset to match the skew energy
    int tailBaseX = body.center().x() + 10;  // slightly off-center
    QPainterPath tailPath;
    if (m_tailDown) {
        tailPath.moveTo(tailBaseX - TAIL_WIDTH / 2.0, body.bottom());
        tailPath.lineTo(tailBaseX + 2, body.bottom() + TAIL_HEIGHT);  // tip slightly right
        tailPath.lineTo(tailBaseX + TAIL_WIDTH / 2.0, body.bottom());
    } else {
        tailPath.moveTo(tailBaseX - TAIL_WIDTH / 2.0, body.top());
        tailPath.lineTo(tailBaseX + 2, body.top() - TAIL_HEIGHT);
        tailPath.lineTo(tailBaseX + TAIL_WIDTH / 2.0, body.top());
    }
    tailPath.closeSubpath();

    QPainterPath combined = bubblePath.united(tailPath);

    // Draw bold shadow: dark, offset down-right for a punchy "pop" effect
    painter.save();
    painter.setOpacity(m_opacity * 0.35);
    painter.setPen(Qt::NoPen);
    QPainterPath shadowPath = combined;
    shadowPath.translate(3, 4);
    painter.setBrush(Qt::black);
    painter.drawPath(shadowPath);
    painter.restore();

    // Draw bubble: white fill + thick black border
    painter.save();
    painter.setOpacity(m_opacity);
    painter.setPen(QPen(Qt::black, BORDER_WIDTH, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(Qt::white);
    painter.drawPath(combined);
    painter.restore();

    // Red accent stripe at top of bubble
    painter.save();
    painter.setOpacity(m_opacity);
    painter.setClipPath(combined);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xE0, 0x1A, 0x2B));  // Persona red
    painter.drawRect(QRectF(body.left(), body.top(), body.width() + sk, 4));
    painter.restore();

    // Offset for text drawing (shifted by shadow margin)
    const int ox = SHADOW_BLUR;
    const int oy = SHADOW_BLUR;

    // Draw title text — bold, black
    if (!m_title.isEmpty()) {
        painter.save();
        painter.setOpacity(m_opacity);
        QFont titleFont("HarmonyOS Sans SC", 12, QFont::Black);
        painter.setFont(titleFont);
        painter.setPen(Qt::black);
        painter.drawText(m_titleRect.translated(ox, oy),
                         Qt::AlignLeft | Qt::AlignVCenter, m_title);
        painter.restore();
    }

    // Draw message text
    if (!m_message.isEmpty()) {
        painter.save();
        painter.setOpacity(m_opacity);
        QFont msgFont("HarmonyOS Sans SC", 12);
        painter.setFont(msgFont);
        painter.setPen(QColor(0x1A, 0x1A, 0x1A));
        QFontMetrics fm(msgFont);
        QString wrappedText = fm.elidedText(m_message, Qt::ElideRight, m_messageRect.width());
        painter.drawText(m_messageRect.translated(ox, oy),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, wrappedText);
        painter.restore();
    }

    // Draw source label — subtle gray
    if (!m_source.isEmpty()) {
        painter.save();
        painter.setOpacity(m_opacity);
        QFont sourceFont("HarmonyOS Sans SC", 10);
        painter.setFont(sourceFont);
        painter.setPen(QColor(0x88, 0x88, 0x88));
        painter.drawText(m_sourceRect.translated(ox, oy),
                         Qt::AlignLeft | Qt::AlignVCenter, m_source);
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

    // Calculate bubble position: center bubble horizontally on pet center
    int bubbleX = petCenterX - width() / 2;

    // Overlap the pet slightly so the tail feels attached, not floating
    constexpr int PET_OVERLAP = 4;
    int bubbleY = petTop - m_bubbleRect.height() - TAIL_HEIGHT - SHADOW_BLUR + PET_OVERLAP;

    m_tailDown = true; // default: tail points down

    // Check if not enough room above
    QScreen *screen = QGuiApplication::screenAt(petCenter);
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        if (bubbleY < screenRect.top()) {
            // Not enough room above — place below pet with same overlap
            bubbleY = petBottom + TAIL_HEIGHT + SHADOW_BLUR - PET_OVERLAP;
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
    QFont titleFont("HarmonyOS Sans SC", 12, QFont::Bold);
    QFont msgFont("HarmonyOS Sans SC", 12);
    QFont sourceFont("HarmonyOS Sans SC", 10);

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
        bubbleHeight += 4; // Spacing between title and message
    }
    if (sourceHeight > 0) {
        bubbleHeight += 4; // Spacing before source label
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
        currentY += 4; // spacing
        m_sourceRect = QRect(PADDING_H, currentY,
                             bubbleWidth - PADDING_H * 2, sourceHeight);
    } else {
        m_sourceRect = QRect();
    }
}

void TipBubbleWidget::updateBubblePath()
{
    // Symmetric tail centered on bubble
    int tailCenterX = m_bubbleRect.width() / 2;

    if (m_tailDown) {
        m_tailPoly.clear();
        m_tailPoly << QPoint(tailCenterX - TAIL_WIDTH / 2, m_bubbleRect.bottom())
                   << QPoint(tailCenterX, m_bubbleRect.bottom() + TAIL_HEIGHT)
                   << QPoint(tailCenterX + TAIL_WIDTH / 2, m_bubbleRect.bottom());
    } else {
        m_tailPoly.clear();
        m_tailPoly << QPoint(tailCenterX - TAIL_WIDTH / 2, m_bubbleRect.top())
                   << QPoint(tailCenterX, m_bubbleRect.top() - TAIL_HEIGHT)
                   << QPoint(tailCenterX + TAIL_WIDTH / 2, m_bubbleRect.top());
    }
}
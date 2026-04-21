#include "SpeechBubble.h"

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QRect>
#include <QPropertyAnimation>
#include <QDebug>

SpeechBubble::SpeechBubble(QObject *parent)
    : QObject(parent)
    , m_scaleAnim(this, "scale")
{
    m_scaleAnim.setDuration(200);
    m_scaleAnim.setEasingCurve(QEasingCurve::OutQuad);

    connect(&m_dismissTimer, &QTimer::timeout, this, &SpeechBubble::hide);
}

SpeechBubble::~SpeechBubble() = default;

void SpeechBubble::showMessage(const QString &title, const QString &message, BubbleType type)
{
    m_title = title;
    m_message = message;
    m_type = type;

    // Calculate layout based on text
    QFont font("Tahoma", 11);
    QFontMetrics fm(font);

    // Calculate text dimensions
    int textWidth = qMax(fm.horizontalAdvance(title), fm.horizontalAdvance(message));
    textWidth = qMin(textWidth, MAX_WIDTH - 2 * BUBBLE_PADDING);
    int textHeight = fm.height() * 2 + 4; // title + message + spacing

    // Calculate bubble rect
    int bubbleWidth = textWidth + 2 * BUBBLE_PADDING;
    int bubbleHeight = textHeight + 2 * BUBBLE_PADDING + TAIL_SIZE;

    m_textRect = QRect(BUBBLE_PADDING, BUBBLE_PADDING, textWidth, textHeight);
    m_bubbleRect = QRect(0, 0, bubbleWidth, bubbleHeight);

    startEnterAnimation();

    // Set auto-dismiss timer
    m_dismissTimer.stop();
    m_dismissTimer.setInterval(m_type == StatusBubble ? m_statusDismissMs : m_tipDismissMs);
    m_dismissTimer.start();
}

void SpeechBubble::hide()
{
    m_dismissTimer.stop();
    startExitAnimation();
}

void SpeechBubble::paint(QPainter *painter, const QRect &petBounds)
{
    if (!m_visible && m_scale <= 0.01) {
        return;
    }

    calculateLayout(petBounds);

    painter->save();

    // Apply scale transform from center of bubble
    if (m_scale != 1.0) {
        QPointF center = m_bubbleRect.center();
        painter->translate(center);
        painter->scale(m_scale, m_scale);
        painter->translate(-center);
    }

    // Draw drop shadow
    QRect shadowRect = m_bubbleRect.translated(DROP_SHADOW_OFFSET, DROP_SHADOW_OFFSET);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(128, 128, 128, 80)); // #808080 at ~30% opacity
    painter->drawRoundedRect(shadowRect, BUBBLE_RADIUS, BUBBLE_RADIUS);

    // Draw bubble background
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(QColor(255, 255, 225)); // #FFFFE1 - Win98 tooltip yellow
    painter->drawRoundedRect(m_bubbleRect, BUBBLE_RADIUS, BUBBLE_RADIUS);

    // Draw triangular tail
    QPolygon tail;
    if (m_tailUp) {
        // Tail points downward from bottom of bubble
        tail << QPoint(m_tailTip.x() - TAIL_SIZE, m_bubbleRect.bottom())
             << m_tailTip
             << QPoint(m_tailTip.x() + TAIL_SIZE, m_bubbleRect.bottom());
    } else {
        // Tail points upward from top of bubble
        tail << QPoint(m_tailTip.x() - TAIL_SIZE, m_bubbleRect.top())
             << m_tailTip
             << QPoint(m_tailTip.x() + TAIL_SIZE, m_bubbleRect.top());
    }
    painter->setBrush(QColor(255, 255, 225));
    painter->drawPolygon(tail);

    // Draw text
    painter->setPen(Qt::black);
    QFont font("Tahoma", 11);
    painter->setFont(font);

    QRect titleRect = m_textRect;
    titleRect.setHeight(QFontMetrics(font).height());
    painter->drawText(titleRect, Qt::TextSingleLine, m_title);

    QRect bodyRect = m_textRect;
    bodyRect.setTop(titleRect.bottom() + 4);
    painter->drawText(bodyRect, Qt::TextWordWrap | Qt::AlignTop, m_message);

    painter->restore();
}

void SpeechBubble::setScale(qreal s)
{
    if (m_scale != s) {
        m_scale = s;
    }
}

void SpeechBubble::calculateLayout(const QRect &petBounds)
{
    // Position bubble above pet by default
    int bubbleX = petBounds.center().x() - m_bubbleRect.width() / 2;
    int bubbleY = petBounds.top() - m_bubbleRect.height() - TAIL_SIZE;

    // Check screen bounds and flip if needed
    m_tailUp = true;

    if (bubbleY < 0) {
        // Not enough room above — place below pet
        bubbleY = petBounds.bottom() + TAIL_SIZE;
        m_tailUp = false;
    }

    // Tail follows pet center horizontally
    m_tailTip = QPoint(petBounds.center().x(),
                       m_tailUp ? petBounds.top() : petBounds.bottom());

    m_bubbleRect.moveTo(bubbleX, bubbleY);
}

void SpeechBubble::startEnterAnimation()
{
    m_visible = true;
    m_scaleAnim.stop();
    m_scaleAnim.setStartValue(0.0);
    m_scaleAnim.setEndValue(1.0);
    m_scaleAnim.setDuration(200);
    m_scaleAnim.setEasingCurve(QEasingCurve::OutQuad);
    m_scaleAnim.start();
}

void SpeechBubble::startExitAnimation()
{
    m_scaleAnim.stop();
    m_scaleAnim.setStartValue(m_scale);
    m_scaleAnim.setEndValue(0.0);
    m_scaleAnim.setDuration(150);
    m_scaleAnim.setEasingCurve(QEasingCurve::InQuad);
    connect(&m_scaleAnim, &QPropertyAnimation::finished, this, [this]() {
        m_visible = false;
    });
    m_scaleAnim.start();
}

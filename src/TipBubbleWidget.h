#ifndef TIPBUBBLEWIDGET_H
#define TIPBUBBLEWIDGET_H

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QPropertyAnimation>
#include <QRect>
#include <QPolygon>

class TipBubbleWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    enum BubbleType { StatusBubble = 0, TipBubble = 1 };

    explicit TipBubbleWidget(QWidget *parent = nullptr);
    ~TipBubbleWidget();

    // Position relative to the pet widget
    void anchorTo(const QWidget *petWidget);
    void setAnchorRect(const QRect &rect) { m_anchorRect = rect; }

    // Show bubble with title + message, optionally showing source label
    void showBubble(const QString &title, const QString &message, BubbleType type = TipBubble, const QString &source = "");

    // Hide with exit animation
    void hideBubble();

    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal o);

    // --- Test accessors ------------------------------------------------------
    QString title() const { return m_title; }
    QString message() const { return m_message; }
    BubbleType bubbleType() const { return m_type; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void positionRelativeTo(const QWidget *pet);
    void startEnterAnimation();
    void startExitAnimation();
    void calculateTextLayout();
    void updateBubblePath();

    QString m_title;
    QString m_message;
    QString m_source;
    BubbleType m_type = TipBubble;

    // Layout
    QRect m_bubbleRect;      // The rounded rectangle part (excludes tail)
    QRect m_titleRect;       // Where title text is drawn
    QRect m_messageRect;      // Where message text is drawn
    QRect m_sourceRect;       // Where source label is drawn
    QPolygon m_tailPoly;      // Tail triangle
    bool m_tailDown = true;   // true = tail points down (bubble above pet)

    // Animation
    qreal m_opacity = 1.0;
    QPropertyAnimation *m_opacityAnim = nullptr;

    // Auto-dismiss
    QTimer m_dismissTimer;
    static constexpr int STATUS_DISMISS_MS = 6000;
    static constexpr int TIP_DISMISS_MS = 12000;

    // Styling
    static constexpr int PADDING_H = 14;       // horizontal padding
    static constexpr int PADDING_V = 10;        // vertical padding
    static constexpr int CORNER_RADIUS = 4;     // sharp-ish corners (P5 feel)
    static constexpr int TAIL_WIDTH = 16;       // base of tail triangle
    static constexpr int TAIL_HEIGHT = 10;      // height of tail triangle
    static constexpr int MAX_BUBBLE_WIDTH = 220;
    static constexpr int SHADOW_BLUR = 10;      // shadow spread
    static constexpr int BORDER_WIDTH = 3;      // bold border
    static constexpr int SKEW_PX = 4;           // parallelogram skew offset

    const QWidget *m_anchoredPet = nullptr;
    QRect m_anchorRect;  // rect within the anchored widget to anchor to (empty = full widget)
};

#endif // TIPBUBBLEWIDGET_H
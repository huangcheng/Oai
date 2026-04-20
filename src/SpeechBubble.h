#ifndef SPEECHBUBBLE_H
#define SPEECHBUBBLE_H

#include <QObject>
#include <QRect>
#include <QTimer>
#include <QPropertyAnimation>

class QPainter;
class QWidget;

class SpeechBubble : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale WRITE setScale)

public:
    enum BubbleType {
        StatusBubble,  // 4 second auto-dismiss
        TipBubble      // 8 second auto-dismiss
    };

    explicit SpeechBubble(QObject *parent = nullptr);
    ~SpeechBubble() override;

    void showMessage(const QString &title, const QString &message,
                     BubbleType type = TipBubble);
    void hide();

    void paint(QPainter *painter, const QRect &petBounds);

    qreal scale() const { return m_scale; }
    void setScale(qreal s);

    bool isVisible() const { return m_visible; }

private:
    void calculateLayout(const QRect &petBounds);
    void startEnterAnimation();
    void startExitAnimation();

    // Bubble content
    QString m_title;
    QString m_message;
    BubbleType m_type = TipBubble;
    bool m_visible = false;

    // Layout
    QRect m_bubbleRect;
    QRect m_textRect;
    QPoint m_tailTip;
    bool m_tailUp = true; // tail points down (bubble above pet)

    // Animation
    qreal m_scale = 1.0;
    QPropertyAnimation m_scaleAnim;

    // Auto-dismiss
    QTimer m_dismissTimer;
    int m_statusDismissMs = 4000;
    int m_tipDismissMs = 8000;

    // Styling constants
    static constexpr int BUBBLE_PADDING = 12;
    static constexpr int BUBBLE_RADIUS = 4;
    static constexpr int TAIL_SIZE = 10;
    static constexpr int MAX_WIDTH = 280;
    static constexpr int DROP_SHADOW_OFFSET = 2;
};

#endif // SPEECHBUBBLE_H

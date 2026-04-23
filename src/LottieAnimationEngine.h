#ifndef LOTTIEANIMATIONENGINE_H
#define LOTTIEANIMATIONENGINE_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <memory>
#include <rlottie.h>

class QPainter;
class QRect;
class QImage;
class SpritePack;

class LottieAnimationEngine : public QObject
{
    Q_OBJECT

public:
    enum Priority {
        HighPriority,   // Interrupts current animation immediately
        NormalPriority  // Queued after current animation
    };

    explicit LottieAnimationEngine(QObject *parent = nullptr);
    ~LottieAnimationEngine() override;

    // Load all Lottie JSON files from assets
    void loadAnimations(const QString &characterDir);

    /**
     * @brief Load animations from a sprite pack
     * @param pack Sprite pack to load from
     * @return true if loaded successfully
     */
    bool loadFromSpritePack(const SpritePack *pack);

    // Play named animation with given priority
    void playAnimation(const QString &name, Priority priority = NormalPriority);

    // Render current frame onto painter
    void paint(QPainter *painter, const QRect &bounds);

    // Check if animation is playing
    bool isPlaying() const { return m_playing; }
    
    // Check if engine has any animations loaded
    bool hasAnimations() const { return !m_animations.isEmpty(); }

signals:
    void effectRequested(const QString &effectName);
    void frameChanged(); // emitted every tick so parent widget can repaint

private slots:
    void tick();

private:
    void advanceFrame();
    void startNextAnimation();
    void startIdleAnimation();

    struct AnimationState {
        std::shared_ptr<rlottie::Animation> animation;
        QString name;
        int totalFrames = 0;
        double frameRate = 30.0;
        bool loop = false;
        QString effect;
        QString sound;
    };

    // Loaded animations (name → animation)
    QMap<QString, AnimationState> m_animations;

    // Current playback
    AnimationState m_current;
    int m_currentFrame = 0;
    double m_speedMultiplier = 1.0;
    double m_elapsedMs = 0.0;
    bool m_playing = false;
    bool m_looping = false;

    // Render buffer (shared, reusable)
    std::vector<uint32_t> m_buffer;
    int m_bufferWidth = 200;
    int m_bufferHeight = 200;

    // Timer for frame updates (16ms ≈ 60fps)
    QTimer m_timer;

    // Idle pool
    QStringList m_idleAnims;
    QVector<int> m_idleWeights;
    QTimer m_idleTimer;
    int m_idleTimeoutMs = 3000;

    // Queue for normal priority animations
    QStringList m_queue;
};

#endif // LOTTIEANIMATIONENGINE_H

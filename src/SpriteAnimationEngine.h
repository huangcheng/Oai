#ifndef SPRITEANIMATIONENGINE_H
#define SPRITEANIMATIONENGINE_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QPixmap>
#include <QRect>

class QPainter;
class CharacterPack;

class SpriteAnimationEngine : public QObject
{
    Q_OBJECT

public:
    enum Priority {
        HighPriority,   // Interrupts current animation immediately
        NormalPriority  // Queued after current animation
    };

    explicit SpriteAnimationEngine(QObject *parent = nullptr);
    ~SpriteAnimationEngine() override;

    /**
     * Load sprite sheet and animation definitions.
     * @param spriteSheet  Path to map.png (the sprite sheet image)
     * @param animJson     Path to animations.json (frame region definitions)
     */
    void loadAssets(const QString &spriteSheet, const QString &animJson);

    /**
     * @brief Load animations from a sprite pack
     * @param pack Sprite pack to load from
     * @return true if loaded successfully
     */
    bool loadFromCharacterPack(const CharacterPack *pack);

    // Play named animation with given priority
    void playAnimation(const QString &name, Priority priority = NormalPriority);

    // Stop playback and clear state (used when switching to a different engine).
    void stop();

    // Render current frame onto painter
    void paint(QPainter *painter, const QRect &bounds);

    // --- Test accessors ------------------------------------------------------
    QString currentAnimation() const { return m_current.name; }
    bool isPlaying() const { return m_playing; }
    int queueSize() const { return m_queue.size(); }

    /** @brief True once loadAssets / loadFromCharacterPack has loaded animations. */
    bool hasAnimations() const { return !m_animations.isEmpty(); }

signals:
    void effectRequested(const QString &effectName);
    void frameChanged();

private slots:
    void tick();

private:
    void advanceFrame();
    void startNextAnimation();
    void startIdleAnimation();
    void checkEffectTrigger(const QString &animName);

    struct FrameDef {
        QRect sourceRect;  // Region in the sprite sheet
        int durationMs;
    };

    struct AnimationDef {
        QString name;
        QVector<FrameDef> frames;
        int totalDurationMs = 0;
        bool loop = false;
        QString effect;
        QString sound;
    };

    // All loaded animations
    QMap<QString, AnimationDef> m_animations;

    // The sprite sheet image
    QPixmap m_spriteSheet;
    int m_frameWidth = 120;
    int m_frameHeight = 120;

    // Current playback
    AnimationDef m_current;
    int m_currentFrameIndex = 0;
    int m_currentFrameElapsed = 0;
    bool m_playing = false;
    bool m_looping = false;

    // Timer (16ms ≈ 60fps)
    QTimer m_timer;

    // Idle pool
    QStringList m_idleAnims;
    QVector<int> m_idleWeights;
    QTimer m_idleTimer;
    int m_idleTimeoutMs = 3000;

    // Queue
    QStringList m_queue;

    // Name mapping: lowercase/snake_case → PascalCase from animations.json
    QMap<QString, QString> m_nameMap;
    void buildNameMap();
};

#endif // SPRITEANIMATIONENGINE_H

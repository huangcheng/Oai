#ifndef LOTTIEEFFECTOVERLAY_H
#define LOTTIEEFFECTOVERLAY_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QPointF>
#include <memory>
#include <rlottie.h>

class QPainter;
class QRect;

class LottieEffectOverlay : public QObject
{
    Q_OBJECT

public:
    explicit LottieEffectOverlay(QObject *parent = nullptr);
    ~LottieEffectOverlay() override;

    // Load effect Lottie files from directory
    void loadEffects(const QString &effectsDir);

    // Trigger an effect by name
    void triggerEffect(const QString &effectName);

    // Stop a looping effect
    void stopEffect(const QString &effectName);

    // Render all active effects
    void paint(QPainter *painter, const QRect &petBounds);

private slots:
    void tick();

private:
    struct EffectConfig {
        QPointF offset;  // Position offset relative to pet center
        bool loop = false;
    };

    struct ActiveEffect {
        std::shared_ptr<rlottie::Animation> animation;
        QString name;
        int currentFrame = 0;
        int totalFrames = 0;
        double frameRate = 30.0;
        double elapsedMs = 0.0;
        bool loop = false;
        QPointF offset;
        std::vector<uint32_t> buffer;
        int width = 200;
        int height = 200;
    };

    // Loaded effect templates (name → animation)
    QMap<QString, std::shared_ptr<rlottie::Animation>> m_effectTemplates;

    // Per-effect configuration
    QMap<QString, EffectConfig> m_configs;

    // Currently active effects
    QVector<ActiveEffect> m_activeEffects;

    // Frame timer
    QTimer m_timer;

    void setupDefaults();
};

#endif // LOTTIEEFFECTOVERLAY_H

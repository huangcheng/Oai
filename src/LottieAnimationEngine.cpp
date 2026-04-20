#include "LottieAnimationEngine.h"

#include <QPainter>
#include <QImage>
#include <QDir>
#include <QFileInfoList>
#include <QRandomGenerator>
#include <QDebug>

LottieAnimationEngine::LottieAnimationEngine(QObject *parent)
    : QObject(parent)
    , m_timer(this)
    , m_idleTimer(this)
{
    m_buffer.resize(m_bufferWidth * m_bufferHeight);
    std::fill(m_buffer.begin(), m_buffer.end(), 0);

    m_timer.setInterval(16); // ~60fps
    connect(&m_timer, &QTimer::timeout, this, &LottieAnimationEngine::tick);

    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(m_idleTimeoutMs);
    connect(&m_idleTimer, &QTimer::timeout, this, &LottieAnimationEngine::startIdleAnimation);
}

LottieAnimationEngine::~LottieAnimationEngine() = default;

void LottieAnimationEngine::loadAnimations(const QString &characterDir)
{
    QDir dir(characterDir);
    if (!dir.exists()) {
        qWarning() << "Character animation directory not found:" << characterDir;
        return;
    }

    const QFileInfoList files = dir.entryInfoList({"*.json"}, QDir::Files);
    for (const QFileInfo &fi : files) {
        const QString name = fi.baseName();
        auto anim = rlottie::Animation::loadFromFile(fi.absoluteFilePath().toStdString());
        if (!anim) {
            qWarning() << "Failed to load Lottie animation:" << fi.fileName();
            continue;
        }

        AnimationState state;
        state.animation = std::move(anim);
        state.name = name;
        state.totalFrames = static_cast<int>(state.animation->totalFrame());
        state.frameRate = state.animation->frameRate();

        m_animations.insert(name, state);
        qDebug() << "Loaded animation:" << name << "frames:" << state.totalFrames;

        // Add to idle pool if name starts with "idle"
        if (name.startsWith("idle")) {
            m_idleAnims.append(name);
            m_idleWeights.append(3); // equal weight
        }
    }

    // Start with rest pose
    if (m_animations.contains("rest")) {
        playAnimation("rest", HighPriority);
    }

    m_timer.start();
}

void LottieAnimationEngine::playAnimation(const QString &name, Priority priority)
{
    if (!m_animations.contains(name)) {
        qWarning() << "Animation not found:" << name;
        return;
    }

    // Reset idle timer on any animation request
    m_idleTimer.stop();

    if (priority == HighPriority) {
        // Interrupt current animation immediately
        m_current = m_animations.value(name);
        m_currentFrame = 0;
        m_elapsedMs = 0.0;
        m_playing = true;
        m_looping = (name == "rest");
        m_queue.clear(); // Clear queued animations
    } else {
        // Queue after current
        if (!m_playing) {
            m_current = m_animations.value(name);
            m_currentFrame = 0;
            m_elapsedMs = 0.0;
            m_playing = true;
            m_looping = false;
        } else {
            m_queue.append(name);
        }
    }
}

void LottieAnimationEngine::paint(QPainter *painter, const QRect &bounds)
{
    if (!m_playing || !m_current.animation) {
        return;
    }

    rlottie::Surface surface(m_buffer.data(), m_bufferWidth, m_bufferHeight,
                              m_bufferWidth * sizeof(uint32_t));
    m_current.animation->renderSync(m_currentFrame, surface);

    QImage image(reinterpret_cast<const uchar*>(m_buffer.data()),
                 m_bufferWidth, m_bufferHeight,
                 m_bufferWidth * sizeof(uint32_t),
                 QImage::Format_ARGB32_Premultiplied);

    painter->drawImage(bounds, image);
}

void LottieAnimationEngine::tick()
{
    if (!m_playing || !m_current.animation) {
        return;
    }

    advanceFrame();
    emit frameChanged();
}

void LottieAnimationEngine::advanceFrame()
{
    m_elapsedMs += 16.0; // 16ms per tick

    const double frameDuration = 1000.0 / (m_current.frameRate * m_speedMultiplier);
    if (m_elapsedMs < frameDuration) {
        return; // Not enough time elapsed for next frame
    }

    m_elapsedMs -= frameDuration;
    m_currentFrame++;

    if (m_currentFrame >= m_current.totalFrames) {
        if (m_looping) {
            m_currentFrame = 0;
        } else {
            startNextAnimation();
        }
    }
}

void LottieAnimationEngine::startNextAnimation()
{
    if (!m_queue.isEmpty()) {
        const QString nextName = m_queue.takeFirst();
        m_current = m_animations.value(nextName);
        m_currentFrame = 0;
        m_elapsedMs = 0.0;
        m_playing = true;
        m_looping = false;

        // Check if this animation should trigger an effect
        const QStringList effectTriggers = {"congratulate", "alert", "sendmail"};
        if (effectTriggers.contains(nextName)) {
            QString effectName;
            if (nextName == "congratulate") effectName = "confetti";
            else if (nextName == "alert") effectName = "alert-pulse";
            else if (nextName == "sendmail") effectName = "sparkles";

            if (!effectName.isEmpty()) {
                emit effectRequested(effectName);
            }
        }
    } else {
        // No more animations — go to rest pose
        m_playing = false;
        m_idleTimer.start();
    }
}

void LottieAnimationEngine::startIdleAnimation()
{
    if (m_idleAnims.isEmpty()) {
        playAnimation("rest", HighPriority);
        return;
    }

    // Weighted random selection
    int totalWeight = 0;
    for (int w : m_idleWeights) {
        totalWeight += w;
    }

    int roll = QRandomGenerator::global()->bounded(totalWeight);
    int cumulative = 0;
    for (int i = 0; i < m_idleAnims.size(); ++i) {
        cumulative += m_idleWeights.at(i);
        if (roll < cumulative) {
            playAnimation(m_idleAnims.at(i), HighPriority);
            return;
        }
    }

    playAnimation(m_idleAnims.first(), HighPriority);
}

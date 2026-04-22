#include "SpriteAnimationEngine.h"

#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QRandomGenerator>
#include <QDebug>

SpriteAnimationEngine::SpriteAnimationEngine(QObject *parent)
    : QObject(parent)
    , m_timer(this)
    , m_idleTimer(this)
{
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &SpriteAnimationEngine::tick);

    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(m_idleTimeoutMs);
    connect(&m_idleTimer, &QTimer::timeout, this, &SpriteAnimationEngine::startIdleAnimation);

    buildNameMap();
}

SpriteAnimationEngine::~SpriteAnimationEngine() = default;

void SpriteAnimationEngine::buildNameMap()
{
    // Map lowercase/snake_case names → PascalCase names in animations.json
    m_nameMap["rest"]            = "RestPose";
    m_nameMap["idle1"]           = "Idle1_1";
    m_nameMap["idle2"]           = "IdleAtom";
    m_nameMap["idle3"]           = "IdleSideToSide";
    m_nameMap["click1"]          = "GestureUp";
    m_nameMap["click2"]          = "GestureRight";
    m_nameMap["doubleclick1"]    = "GetAttention";
    m_nameMap["doubleclick2"]    = "GetArtsy";
    m_nameMap["gesture_down"]    = "GestureDown";
    m_nameMap["gesture_up"]      = "GestureUp";
    m_nameMap["gesture_left"]    = "GestureLeft";
    m_nameMap["gesture_right"]   = "GestureRight";
    m_nameMap["alert"]           = "Alert";
    m_nameMap["congratulate"]    = "Congratulate";
    m_nameMap["explain"]         = "Explain";
    m_nameMap["getattentionyawn"]= "GetAttention";
    m_nameMap["lookdown"]        = "LookDown";
    m_nameMap["lookleft"]        = "LookLeft";
    m_nameMap["lookright"]       = "LookRight";
    m_nameMap["lookup"]          = "LookUp";
    m_nameMap["sendmail"]        = "SendMail";
    m_nameMap["thinking"]        = "Thinking";
    m_nameMap["wave"]            = "Wave";
    m_nameMap["greeting"]        = "Greeting";
    m_nameMap["goodbye"]         = "GoodBye";
    m_nameMap["processing"]      = "Processing";
    m_nameMap["writing"]         = "Writing";
    m_nameMap["searching"]       = "Searching";
    m_nameMap["print"]           = "Print";
    m_nameMap["save"]            = "Save";
    m_nameMap["hide"]            = "Hide";
    m_nameMap["show"]            = "Show";
    m_nameMap["gettechy"]        = "GetTechy";
    m_nameMap["getwizardy"]      = "GetWizardy";
}

void SpriteAnimationEngine::loadAssets(const QString &spriteSheetPath, const QString &animJsonPath)
{
    // Load sprite sheet
    if (!m_spriteSheet.load(spriteSheetPath)) {
        qWarning() << "Failed to load sprite sheet:" << spriteSheetPath;
        return;
    }
    qDebug() << "Sprite sheet loaded:" << spriteSheetPath
             << "size:" << m_spriteSheet.size();

    // Determine frame dimensions from sprite sheet and animation data
    // Original Clippy: 27 cols x 34 rows, 124x93 per frame
    m_frameWidth = 124;
    m_frameHeight = 93;

    // Load animation definitions
    QFile file(animJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open animations.json:" << animJsonPath;
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "animations.json parse error:" << error.errorString();
        return;
    }

    QJsonArray anims = doc.array();
    for (const QJsonValue &val : anims) {
        QJsonObject animObj = val.toObject();
        const QString name = animObj["Name"].toString();

        AnimationDef def;
        def.name = name;

        QJsonArray frames = animObj["Frames"].toArray();
        for (const QJsonValue &fval : frames) {
            QJsonObject frameObj = fval.toObject();
            QJsonObject offsets = frameObj["ImagesOffsets"].toObject();

            // Some frames have null/empty offsets (e.g., blank transition frames)
            if (offsets.isEmpty() || !offsets.contains("Column")) {
                // Use first frame as fallback
                offsets["Column"] = 0;
                offsets["Row"] = 0;
            }

            int col = offsets["Column"].toInt();
            int row = offsets["Row"].toInt();
            int duration = frameObj["Duration"].toInt();

            FrameDef frame;
            frame.sourceRect = QRect(col * m_frameWidth, row * m_frameHeight,
                                     m_frameWidth, m_frameHeight);
            frame.durationMs = duration;
            def.frames.append(frame);
            def.totalDurationMs += duration;
        }

        m_animations.insert(name, def);
    }

    qDebug() << "Loaded" << m_animations.size() << "animations";

    // Build idle pool from idle-type animations
    // {name, weight} — higher weight = more frequent
    const QVector<QPair<QString, int>> idlePool = {
        // Classic idle animations
        {"Idle1_1",           4},
        {"IdleAtom",          3},
        {"IdleSideToSide",    3},
        {"IdleRopePile",      2},
        {"IdleHeadScratch",   3},
        {"IdleFingerTap",     3},
        {"IdleEyeBrowRaise",  3},
        {"IdleSnooze",        1},
        // Look-around animations
        {"LookLeft",          2},
        {"LookRight",         2},
        {"LookUp",            2},
        {"LookDown",          2},
        {"LookUpLeft",        1},
        {"LookUpRight",       1},
        {"LookDownLeft",      1},
        {"LookDownRight",     1},
        // Personality animations
        {"GetArtsy",          1},
        {"GetTechy",          1},
        {"GetWizardy",        1},
        {"Hearing_1",         1},
        {"CheckingSomething", 1},
        {"Writing",           1},
        {"Searching",         1},
    };
    for (const auto &[name, weight] : idlePool) {
        if (m_animations.contains(name)) {
            m_idleAnims.append(name);
            m_idleWeights.append(weight);
        }
    }

    // Start with RestPose (non-looping — idle timer will cycle animations)
    if (m_animations.contains("RestPose")) {
        playAnimation("RestPose", HighPriority);
        // After RestPose finishes, idle timer kicks in
    }

    m_timer.start();
    // Start idle timer — first idle animation plays after 3 seconds
    m_idleTimer.start();
}

void SpriteAnimationEngine::playAnimation(const QString &name, Priority priority)
{
    // Resolve name via mapping
    QString actualName = m_nameMap.value(name, name);

    if (!m_animations.contains(actualName)) {
        qWarning() << "Animation not found:" << name << "(resolved to" << actualName << ")";
        return;
    }

    // If the same animation is already playing, let it finish — don't restart.
    if (m_playing && m_current.name == actualName) {
        return;
    }

    m_idleTimer.stop();

    // Idle and RestPose animations should always be interruptible by events
    bool currentIsIdle = m_idleAnims.contains(m_current.name)
                         || m_current.name == "RestPose";

    if (priority == HighPriority || currentIsIdle) {
        m_current = m_animations.value(actualName);
        m_currentFrameIndex = 0;
        m_currentFrameElapsed = 0;
        m_playing = true;
        m_looping = false;  // No animation loops — idle timer handles cycling
        if (priority == HighPriority) {
            m_queue.clear();
        }
    } else {
        if (!m_playing) {
            m_current = m_animations.value(actualName);
            m_currentFrameIndex = 0;
            m_currentFrameElapsed = 0;
            m_playing = true;
            m_looping = false;
        } else {
            m_queue.append(actualName);
        }
    }
}

void SpriteAnimationEngine::paint(QPainter *painter, const QRect &bounds)
{
    if (m_playing && !m_current.frames.isEmpty()) {
        // Draw current animation frame
        const FrameDef &frame = m_current.frames.at(m_currentFrameIndex);
        painter->drawPixmap(bounds, m_spriteSheet, frame.sourceRect);
    } else if (m_animations.contains("RestPose")) {
        // Always show RestPose when no animation is playing
        const FrameDef &frame = m_animations["RestPose"].frames.first();
        painter->drawPixmap(bounds, m_spriteSheet, frame.sourceRect);
    }
}

void SpriteAnimationEngine::tick()
{
    if (!m_playing || m_current.frames.isEmpty()) {
        return;
    }

    advanceFrame();
    emit frameChanged();
}

void SpriteAnimationEngine::advanceFrame()
{
    m_currentFrameElapsed += 16; // 16ms per tick

    const FrameDef &frame = m_current.frames.at(m_currentFrameIndex);
    if (m_currentFrameElapsed >= frame.durationMs) {
        m_currentFrameElapsed -= frame.durationMs;
        m_currentFrameIndex++;

        if (m_currentFrameIndex >= m_current.frames.size()) {
            if (m_looping) {
                m_currentFrameIndex = 0;
            } else {
                startNextAnimation();
            }
        }
    }
}

void SpriteAnimationEngine::startNextAnimation()
{
    if (!m_queue.isEmpty()) {
        const QString nextName = m_queue.takeFirst();
        m_current = m_animations.value(nextName);
        m_currentFrameIndex = 0;
        m_currentFrameElapsed = 0;
        m_playing = true;
        m_looping = false;

        // Check for effect triggers
        const QStringList effectTriggers = {"Congratulate", "Alert", "SendMail"};
        if (effectTriggers.contains(nextName)) {
            QString effectName;
            if (nextName == "Congratulate") effectName = "confetti";
            else if (nextName == "Alert") effectName = "alert-pulse";
            else if (nextName == "SendMail") effectName = "sparkles";

            if (!effectName.isEmpty()) {
                emit effectRequested(effectName);
            }
        }
    } else {
        m_playing = false;
        m_idleTimer.start();
    }
}

void SpriteAnimationEngine::startIdleAnimation()
{
    if (m_idleAnims.isEmpty()) {
        if (m_animations.contains("RestPose")) {
            playAnimation("RestPose", HighPriority);
        }
        return;
    }

    int totalWeight = 0;
    for (int w : m_idleWeights) totalWeight += w;

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

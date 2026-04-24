#ifndef LIVE2DANIMATIONENGINE_H
#define LIVE2DANIMATIONENGINE_H

#ifdef OAI_LIVE2D_SUPPORT

#include <QObject>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include <CubismFramework.hpp>
#include <Model/CubismUserModel.hpp>
#include <CubismModelSettingJson.hpp>
#include <Motion/CubismMotion.hpp>
#include <ICubismModelSetting.hpp>

class QPainter;
class QRect;
class CharacterPack;

/**
 * @brief Live2D Cubism animation engine.
 *
 * Loads a Live2D model from a sprite pack, renders it to an offscreen
 * OpenGL framebuffer, and exposes the result as a QImage for QPainter-based
 * compositing.  Follows the same public interface as the legacy
 * SpriteAnimationEngine so MainWindow / EventRouter can treat all engines
 * uniformly.
 */
class Live2DAnimationEngine : public QObject
{
    Q_OBJECT

public:
    enum Priority {
        HighPriority,   // Interrupts current motion immediately
        NormalPriority  // Queued after current motion
    };

    explicit Live2DAnimationEngine(QObject *parent = nullptr);
    ~Live2DAnimationEngine() override;

    /**
     * @brief Load a Live2D model from a sprite pack.
     * @param pack  Pack whose characterConfig().engineType == Live2D
     * @return true on success
     */
    bool loadFromCharacterPack(const CharacterPack *pack);

    /**
     * @brief Start playing a named motion group.
     *
     * Motion names correspond to groups in model3.json (e.g. "Idle", "TapBody").
     * They are also the keys used in the pack's eventMap.
     */
    void playAnimation(const QString &name, Priority priority = NormalPriority);

    /** @brief Stop playback and clear state (used when switching to a different engine). */
    void stop();

    /** @brief Render the current frame into a QPainter target rect. */
    void paint(QPainter *painter, const QRect &bounds);

    bool isPlaying() const { return m_playing; }
    bool hasAnimations() const { return m_modelLoaded; }

    /** @brief Render dimensions (from manifest frameWidth/frameHeight). */
    int renderWidth() const { return m_renderWidth; }
    int renderHeight() const { return m_renderHeight; }

signals:
    void frameChanged();
    void effectRequested(const QString &effectName);

private slots:
    void tick();

private:
    // --- Cubism helpers ---
    bool initOpenGL();
    void releaseOpenGL();
    bool loadModel(const QString &modelJsonPath, const QString &modelDir);
    void releaseModel();
    void renderFrame();

    void startNextAnimation();
    void startIdleAnimation();

    // --- Cubism SDK objects ---
    // Using raw pointers because CubismUserModel hierarchy manages its own lifetime.
    class CubismModel;                     // forward-declared pimpl
    CubismModel *m_cubismModel = nullptr;  // pimpl hides Cubism headers from consumers

    // --- Render state ---
    QOpenGLContext *m_glContext = nullptr;
    QOffscreenSurface *m_surface = nullptr;
    QOpenGLFramebufferObject *m_fbo = nullptr;
    QImage m_image;
    int m_renderWidth = 200;
    int m_renderHeight = 200;

    // --- Playback state ---
    bool m_modelLoaded = false;
    bool m_playing = false;
    QStringList m_motionGroups;     // available motion group names
    QString m_currentMotion;        // currently playing motion group
    QStringList m_queue;            // queued motion group names

    // --- Timing ---
    QTimer m_timer;
    qint64 m_lastTickMs = 0;

    // --- Idle pool ---
    QStringList m_idleAnims;
    QVector<int> m_idleWeights;
    QTimer m_idleTimer;
    int m_idleTimeoutMs = 3000;
};

#endif // OAI_LIVE2D_SUPPORT

#endif // LIVE2DANIMATIONENGINE_H

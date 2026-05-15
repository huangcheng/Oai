#ifndef SEELIE_ANIMATIONENGINE_H
#define SEELIE_ANIMATIONENGINE_H

#include <QObject>
#include <QString>

class QPainter;
class QRect;
class CharacterPack;

/**
 * Abstract base for the three animation engines (Sprite, Lottie, Live2D).
 *
 * Each concrete engine has its own Qt object hierarchy / threading / asset
 * format, but they share the same public surface used by EventRouter,
 * PetStateMachine, and the IPC-tip fan-out in main.cpp:
 *
 *   loadFromCharacterPack(pack)   — replace current asset set
 *   playAnimation(name, priority) — queue or interrupt a named animation
 *   stop()                        — interrupt the currently playing one
 *   paint(painter, bounds)        — render the current frame
 *   isPlaying()                   — currently emitting frames?
 *   hasAnimations()               — anything loaded at all?
 *   lastPaintSuccessful()         — used by MainWindow's fallback logic
 *   frameChanged() signal         — tells the parent widget to repaint
 *   effectRequested(name) signal  — fires overlay effects (sparkle, etc.)
 *
 * AnimationEngine is NOT a Q_OBJECT (Qt MOC requires concrete subclasses
 * to declare their own Q_OBJECT). Concrete engines inherit BOTH QObject
 * and AnimationEngine; this base just lets call sites that don't need
 * engine-specific extras (playAnimationChain, setPointerTarget) work
 * polymorphically. Audit H3.
 *
 * Engine-specific extras (Live2D's pointer tracking + animation chains)
 * stay on the concrete class — callers cast when they need them.
 */
class AnimationEngine
{
public:
    // Order matches the per-engine Priority enums (HighPriority=0,
    // NormalPriority=1) so the implicit reinterpret across enums in
    // legacy call sites stays correct during the H3 migration.
    enum Priority {
        HighPriority,
        NormalPriority,
    };

    virtual ~AnimationEngine() = default;

    virtual bool loadFromCharacterPack(const CharacterPack *pack) = 0;
    virtual void playAnimation(const QString &name, Priority priority) = 0;
    virtual void stop() = 0;
    virtual void paint(QPainter *painter, const QRect &bounds) = 0;
    virtual bool isPlaying() const = 0;
    virtual bool hasAnimations() const = 0;
    virtual bool lastPaintSuccessful() const { return true; }
};

#endif // SEELIE_ANIMATIONENGINE_H

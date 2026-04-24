#ifndef CHARACTERPACK_H
#define CHARACTERPACK_H

#include <QString>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

/**
 * @brief Represents a loaded sprite pack with all its assets and configurations.
 *
 * A sprite pack contains:
 * - Character animations (Lottie or sprite sheet)
 * - Event-to-animation mappings
 * - Effect triggers
 * - Idle pool configuration
 * - Sound effects
 *
 * Packs can be loaded from:
 * - Directory (for development/hot-reload)
 * - .opk archive (for distribution)
 */
class CharacterPack
{
public:
    /**
     * @brief Animation engine type
     */
    enum class EngineType {
        Lottie,      ///< Lottie JSON vector animations
        SpriteSheet, ///< Sprite sheet with frame grid
        Live2D       ///< Live2D Cubism model
    };

    /**
     * @brief Single animation frame definition (for sprite sheet type)
     */
    struct FrameDef {
        int col = 0;      ///< Column in sprite sheet grid
        int row = 0;      ///< Row in sprite sheet grid
        int x = -1;       ///< Explicit X position (for atlas mode)
        int y = -1;       ///< Explicit Y position (for atlas mode)
        int w = -1;       ///< Explicit width (for atlas mode)
        int h = -1;       ///< Explicit height (for atlas mode)
        int durationMs = 100; ///< Frame duration in milliseconds

        bool isGridMode() const { return x < 0; }
    };

    /**
     * @brief Animation definition
     */
    struct AnimationDef {
        QString name;                    ///< Animation name
        EngineType type = EngineType::Lottie;
        QString lottieFile;              ///< Path to Lottie JSON (for Lottie type)
        QVector<FrameDef> frames;        ///< Frame definitions (for sprite sheet type)
        bool loop = false;               ///< Whether animation loops
        bool highPriority = false;       ///< High priority (interrupts current)
        QString effect;                  ///< Effect to trigger on start
        QString sound;                   ///< Sound to play on start
        int totalDurationMs = 0;         ///< Total animation duration
    };

    /**
     * @brief Idle pool entry with weight
     */
    struct IdleEntry {
        QString animationName;
        int weight = 1;
    };

    /**
     * @brief Sprite pack metadata
     */
    struct Metadata {
        QString formatVersion;    ///< Pack format version
        QString id;               ///< Unique identifier (reverse-domain)
        QString name;             ///< Display name
        QString author;           ///< Creator name
        QString version;          ///< Pack version
        QString description;      ///< Short description
        QString preview;          ///< Path to preview image
        QStringList tags;         ///< Search/discovery tags
        QString license;          ///< SPDX license identifier
        QString minAppVersion;    ///< Minimum Oai version required
    };

    /**
     * @brief Character configuration
     */
    struct CharacterConfig {
        EngineType engineType = EngineType::Lottie;
        QString spriteSheet;      ///< Path to sprite sheet (for SpriteSheet type)
        int frameWidth = 0;       ///< Frame/render width
        int frameHeight = 0;      ///< Frame/render height
        QString animDirectory;    ///< Directory containing Lottie files (for Lottie type)
        QString definitions;      ///< Path to animations.json (for SpriteSheet type, optional)
        QString modelJson;        ///< Path to .model3.json (for Live2D type)
        float displayScale = 1.0f;///< Window-size multiplier over frameWidth/Height (default 1.0).
                                  ///< Sprite sheets can set e.g. 2.0 to render the 124×93 art at 248×186
                                  ///< so it looks comparable in size to 300×300 Live2D packs.
    };

    CharacterPack() = default;
    ~CharacterPack() = default;

    /**
     * @brief Load sprite pack from directory
     * @param packDir Path to pack directory (must contain manifest.json)
     * @return true if loaded successfully
     */
    bool loadFromDirectory(const QString &packDir);

    /**
     * @brief Load sprite pack from .opk archive
     * @param archivePath Path to .opk file
     * @param extractDir Directory to extract to (temp)
     * @return true if loaded successfully
     */
    bool loadFromArchive(const QString &archivePath, const QString &extractDir);

    /**
     * @brief Check if pack is valid and loaded
     */
    bool isValid() const { return m_valid; }

    /**
     * @brief Get pack root directory
     */
    QString rootPath() const { return m_rootPath; }

    /**
     * @brief Get metadata
     */
    const Metadata &metadata() const { return m_metadata; }

    /**
     * @brief Get character configuration
     */
    const CharacterConfig &characterConfig() const { return m_characterConfig; }

    /**
     * @brief Get all animations
     */
    const QMap<QString, AnimationDef> &animations() const { return m_animations; }

    /**
     * @brief Get animation by name
     * @return Pointer to animation, or nullptr if not found
     */
    const AnimationDef *animation(const QString &name) const;

    /**
     * @brief Get event-to-animation mapping
     */
    const QMap<QString, QString> &eventMap() const { return m_eventMap; }

    /**
     * @brief Get effect triggers (animation name -> effect name)
     */
    const QMap<QString, QString> &effectTriggers() const { return m_effectTriggers; }

    /**
     * @brief Get idle pool
     */
    const QVector<IdleEntry> &idlePool() const { return m_idlePool; }

    /**
     * @brief Get absolute path to asset file
     * @param relativePath Relative path from pack root
     * @return Absolute path
     */
    QString assetPath(const QString &relativePath) const;

    /**
     * @brief Get absolute path to Lottie animation file
     * @param animationName Animation name
     * @return Absolute path to Lottie JSON, or empty if not found
     */
    QString lottieAnimationPath(const QString &animationName) const;

    /**
     * @brief Get absolute path to model JSON file (for Live2D type)
     * @return Absolute path to .model3.json, or empty if not Live2D type
     */
    QString modelJsonPath() const;

    /**
     * @brief Get absolute path to effect file
     * @param effectName Effect name
     * @return Absolute path to effect JSON, or empty if not found
     */
    QString effectPath(const QString &effectName) const;

    /**
     * @brief Get absolute path to sound file
     * @param soundName Sound filename
     * @return Absolute path to sound file, or empty if not found
     */
    QString soundPath(const QString &soundName) const;

    /**
     * @brief Get list of available Lottie animation files
     */
    QStringList availableLottieAnimations() const;

    /**
     * @brief Get list of available effect files
     */
    QStringList availableEffects() const;

    /**
     * @brief Get list of available sound files
     */
    QStringList availableSounds() const;

private:
    bool parseManifest(const QJsonObject &manifest);
    bool parseCharacter(const QJsonObject &character);
    bool parseAnimations(const QJsonObject &animations);
    bool parseAnimationDef(const QString &name, const QJsonObject &def, AnimationDef &out);
    bool parseFrames(const QJsonArray &frames, QVector<FrameDef> &out);
    bool parseIdlePool(const QJsonArray &pool);
    bool parseEventMap(const QJsonObject &map);
    bool parseEffectTriggers(const QJsonObject &triggers);
    bool loadAnimationsFromDefinitions(const QString &definitionsPath);

    bool m_valid = false;
    QString m_rootPath;
    Metadata m_metadata;
    CharacterConfig m_characterConfig;
    QMap<QString, AnimationDef> m_animations;
    QMap<QString, QString> m_eventMap;
    QMap<QString, QString> m_effectTriggers;
    QVector<IdleEntry> m_idlePool;
};

#endif // CHARACTERPACK_H

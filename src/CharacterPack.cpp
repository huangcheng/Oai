#include "CharacterPack.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QTemporaryDir>

bool CharacterPack::loadFromDirectory(const QString &packDir)
{
    QDir dir(packDir);
    if (!dir.exists()) {
        qWarning() << "CharacterPack: Directory does not exist:" << packDir;
        return false;
    }

    // Check for manifest.json
    const QString manifestPath = dir.absoluteFilePath("manifest.json");
    if (!QFile::exists(manifestPath)) {
        qWarning() << "CharacterPack: No manifest.json found in:" << packDir;
        return false;
    }

    // Read and parse manifest
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qWarning() << "CharacterPack: Failed to open manifest.json:" << manifestPath;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll(), &error);
    manifestFile.close();

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "CharacterPack: manifest.json parse error:" << error.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "CharacterPack: manifest.json is not an object";
        return false;
    }

    // Parse manifest
    m_rootPath = dir.absolutePath();
    if (!parseManifest(doc.object())) {
        qWarning() << "CharacterPack: Failed to parse manifest";
        return false;
    }

    m_valid = true;
    qDebug() << "CharacterPack: Loaded pack" << m_metadata.name << "from" << m_rootPath;
    return true;
}

bool CharacterPack::loadFromArchive(const QString &archivePath, const QString &extractDir)
{
    // TODO: Implement ZIP extraction using QZipReader or miniz
    // For now, log a warning and return false
    qWarning() << "CharacterPack: Archive loading not yet implemented:" << archivePath;
    return false;
}

const CharacterPack::AnimationDef *CharacterPack::animation(const QString &name) const
{
    auto it = m_animations.find(name);
    if (it != m_animations.end()) {
        return &it.value();
    }
    return nullptr;
}

QString CharacterPack::assetPath(const QString &relativePath) const
{
    if (m_rootPath.isEmpty()) {
        return QString();
    }
    return QDir(m_rootPath).absoluteFilePath(relativePath);
}

QString CharacterPack::lottieAnimationPath(const QString &animationName) const
{
    const AnimationDef *anim = animation(animationName);
    if (!anim || anim->type != EngineType::Lottie) {
        return QString();
    }
    return assetPath(anim->lottieFile);
}

QString CharacterPack::modelJsonPath() const
{
    if (m_characterConfig.engineType != EngineType::Live2D || m_characterConfig.modelJson.isEmpty()) {
        return QString();
    }
    return assetPath(m_characterConfig.modelJson);
}

QString CharacterPack::effectPath(const QString &effectName) const
{
    // Look in effects directory
    const QString effectsDir = assetPath("effects");
    if (effectsDir.isEmpty()) {
        return QString();
    }

    const QString effectFile = QDir(effectsDir).absoluteFilePath(effectName + ".json");
    if (QFile::exists(effectFile)) {
        return effectFile;
    }
    return QString();
}

QString CharacterPack::soundPath(const QString &soundName) const
{
    // Look in sounds directory
    const QString soundsDir = assetPath("sounds");
    if (soundsDir.isEmpty()) {
        return QString();
    }

    const QString soundFile = QDir(soundsDir).absoluteFilePath(soundName);
    if (QFile::exists(soundFile)) {
        return soundFile;
    }
    return QString();
}

QStringList CharacterPack::availableLottieAnimations() const
{
    QStringList result;
    if (m_characterConfig.engineType != EngineType::Lottie) {
        return result;
    }

    const QString animDir = assetPath(m_characterConfig.animDirectory);
    if (animDir.isEmpty()) {
        return result;
    }

    QDir dir(animDir);
    if (!dir.exists()) {
        return result;
    }

    // Scan for .json files
    const QFileInfoList files = dir.entryInfoList({"*.json"}, QDir::Files);
    for (const QFileInfo &fi : files) {
        result.append(fi.baseName());
    }

    return result;
}

QStringList CharacterPack::availableEffects() const
{
    QStringList result;
    const QString effectsDir = assetPath("effects");
    if (effectsDir.isEmpty()) {
        return result;
    }

    QDir dir(effectsDir);
    if (!dir.exists()) {
        return result;
    }

    const QFileInfoList files = dir.entryInfoList({"*.json"}, QDir::Files);
    for (const QFileInfo &fi : files) {
        result.append(fi.baseName());
    }

    return result;
}

QStringList CharacterPack::availableSounds() const
{
    QStringList result;
    const QString soundsDir = assetPath("sounds");
    if (soundsDir.isEmpty()) {
        return result;
    }

    QDir dir(soundsDir);
    if (!dir.exists()) {
        return result;
    }

    const QFileInfoList files = dir.entryInfoList({"*.wav", "*.mp3", "*.ogg"}, QDir::Files);
    for (const QFileInfo &fi : files) {
        result.append(fi.fileName());
    }

    return result;
}

bool CharacterPack::parseManifest(const QJsonObject &manifest)
{
    // Parse format version
    m_metadata.formatVersion = manifest.value("formatVersion").toString();
    if (m_metadata.formatVersion != "1.0.0") {
        qWarning() << "CharacterPack: Unsupported format version:" << m_metadata.formatVersion;
        return false;
    }

    // Parse required fields
    m_metadata.id = manifest.value("id").toString();
    m_metadata.name = manifest.value("name").toString();
    m_metadata.author = manifest.value("author").toString();
    m_metadata.version = manifest.value("version").toString();

    if (m_metadata.id.isEmpty() || m_metadata.name.isEmpty() || 
        m_metadata.author.isEmpty() || m_metadata.version.isEmpty()) {
        qWarning() << "CharacterPack: Missing required metadata fields";
        return false;
    }

    // Parse optional fields
    m_metadata.description = manifest.value("description").toString();
    m_metadata.preview = manifest.value("preview").toString();
    m_metadata.license = manifest.value("license").toString();
    m_metadata.minAppVersion = manifest.value("minAppVersion").toString();

    // Parse tags
    const QJsonArray tagsArray = manifest.value("tags").toArray();
    for (const QJsonValue &tag : tagsArray) {
        m_metadata.tags.append(tag.toString());
    }

    // Parse character configuration
    const QJsonObject characterObj = manifest.value("character").toObject();
    if (!parseCharacter(characterObj)) {
        qWarning() << "CharacterPack: Failed to parse character configuration";
        return false;
    }

    // idlePool / eventMap / effectTriggers live at the top level of the manifest
    // (see schemas/sprite-pack-v1.schema.json and every existing pack).
    const QJsonArray idlePoolArray = manifest.value("idlePool").toArray();
    if (!parseIdlePool(idlePoolArray)) {
        qWarning() << "CharacterPack: Failed to parse idle pool";
        return false;
    }

    const QJsonObject eventMapObj = manifest.value("eventMap").toObject();
    if (!parseEventMap(eventMapObj)) {
        qWarning() << "CharacterPack: Failed to parse event map";
        return false;
    }

    const QJsonObject effectTriggersObj = manifest.value("effectTriggers").toObject();
    if (!parseEffectTriggers(effectTriggersObj)) {
        qWarning() << "CharacterPack: Failed to parse effect triggers";
        return false;
    }

    return true;
}

bool CharacterPack::parseCharacter(const QJsonObject &character)
{
    // Parse engine type
    const QString typeStr = character.value("type").toString();
    if (typeStr == "lottie") {
        m_characterConfig.engineType = EngineType::Lottie;
    } else if (typeStr == "spriteSheet") {
        m_characterConfig.engineType = EngineType::SpriteSheet;
    } else if (typeStr == "live2d") {
        m_characterConfig.engineType = EngineType::Live2D;
    } else {
        qWarning() << "CharacterPack: Unknown character type:" << typeStr;
        return false;
    }

    // Parse type-specific configuration
    if (m_characterConfig.engineType == EngineType::Lottie) {
        m_characterConfig.animDirectory = character.value("directory").toString("character");
    } else if (m_characterConfig.engineType == EngineType::Live2D) {
        m_characterConfig.modelJson = character.value("model").toString();
        m_characterConfig.frameWidth = character.value("frameWidth").toInt(200);
        m_characterConfig.frameHeight = character.value("frameHeight").toInt(200);
    } else {
        m_characterConfig.spriteSheet = character.value("spriteSheet").toString();
        m_characterConfig.frameWidth = character.value("frameWidth").toInt();
        m_characterConfig.frameHeight = character.value("frameHeight").toInt();
        m_characterConfig.definitions = character.value("definitions").toString();

        if (m_characterConfig.spriteSheet.isEmpty() ||
            m_characterConfig.frameWidth <= 0 ||
            m_characterConfig.frameHeight <= 0) {
            qWarning() << "CharacterPack: Invalid sprite sheet configuration";
            return false;
        }
    }

    // Parse animations (from definitions file or inline)
    if (!m_characterConfig.definitions.isEmpty()) {
        // Load animations from external definitions file (original animations.json format)
        const QString definitionsPath = assetPath(m_characterConfig.definitions);
        if (!loadAnimationsFromDefinitions(definitionsPath)) {
            qWarning() << "CharacterPack: Failed to load animations from definitions:" << definitionsPath;
            return false;
        }
    } else {
        // Load animations from inline definitions
        const QJsonObject animationsObj = character.value("animations").toObject();
        if (!parseAnimations(animationsObj)) {
            qWarning() << "CharacterPack: failed to parse animations";
            return false;
        }
    }

    return true;
}

bool CharacterPack::parseAnimations(const QJsonObject &animations)
{
    for (auto it = animations.begin(); it != animations.end(); ++it) {
        const QString name = it.key();
        const QJsonObject animObj = it.value().toObject();

        AnimationDef anim;
        anim.name = name;

        if (!parseAnimationDef(name, animObj, anim)) {
            qWarning() << "CharacterPack: Failed to parse animation:" << name;
            return false;
        }

        m_animations[name] = anim;
    }

    return true;
}

bool CharacterPack::parseAnimationDef(const QString &name, const QJsonObject &def, AnimationDef &out)
{
    // Parse type
    const QString typeStr = def.value("type").toString();
    if (typeStr == "lottie") {
        out.type = EngineType::Lottie;
        out.lottieFile = def.value("file").toString();

        if (out.lottieFile.isEmpty()) {
            qWarning() << "CharacterPack: Lottie animation missing 'file' field:" << name;
            return false;
        }
    } else if (typeStr == "sprite") {
        out.type = EngineType::SpriteSheet;

        const QJsonArray framesArray = def.value("frames").toArray();
        if (!parseFrames(framesArray, out.frames)) {
            qWarning() << "CharacterPack: Failed to parse frames for animation:" << name;
            return false;
        }

        if (out.frames.isEmpty()) {
            qWarning() << "CharacterPack: Sprite animation has no frames:" << name;
            return false;
        }
    } else {
        qWarning() << "CharacterPack: Unknown animation type:" << typeStr << "for" << name;
        return false;
    }

    // Parse optional fields
    out.loop = def.value("loop").toBool(false);
    out.highPriority = def.value("priority").toString() == "high";
    out.effect = def.value("effect").toString();
    out.sound = def.value("sound").toString();

    return true;
}

bool CharacterPack::parseFrames(const QJsonArray &frames, QVector<FrameDef> &out)
{
    for (const QJsonValue &frameVal : frames) {
        const QJsonObject frameObj = frameVal.toObject();
        FrameDef frame;

        frame.durationMs = frameObj.value("duration").toInt(100);

        // Check if grid mode (col/row) or atlas mode (x/y/w/h)
        if (frameObj.contains("col") && frameObj.contains("row")) {
            frame.col = frameObj.value("col").toInt();
            frame.row = frameObj.value("row").toInt();
            frame.x = -1;  // Grid mode
            frame.y = -1;
            frame.w = -1;
            frame.h = -1;
        } else if (frameObj.contains("x") && frameObj.contains("y") && 
                   frameObj.contains("w") && frameObj.contains("h")) {
            frame.x = frameObj.value("x").toInt();
            frame.y = frameObj.value("y").toInt();
            frame.w = frameObj.value("w").toInt();
            frame.h = frameObj.value("h").toInt();
            frame.col = -1;  // Atlas mode
            frame.row = -1;
        } else {
            qWarning() << "CharacterPack: Frame missing position (col/row or x/y/w/h)";
            return false;
        }

        if (frame.durationMs <= 0) {
            qWarning() << "CharacterPack: Frame has invalid duration:" << frame.durationMs;
            return false;
        }

        out.append(frame);
    }

    return true;
}

bool CharacterPack::parseIdlePool(const QJsonArray &pool)
{
    for (const QJsonValue &entryVal : pool) {
        const QJsonObject entryObj = entryVal.toObject();
        IdleEntry entry;

        entry.animationName = entryObj.value("name").toString();
        entry.weight = entryObj.value("weight").toInt(1);

        if (entry.animationName.isEmpty()) {
            qWarning() << "CharacterPack: Idle pool entry missing 'name'";
            return false;
        }

        if (entry.weight <= 0) {
            qWarning() << "CharacterPack: Idle pool entry has invalid weight:" << entry.weight;
            return false;
        }

        m_idlePool.append(entry);
    }

    return true;
}

bool CharacterPack::parseEventMap(const QJsonObject &map)
{
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QString eventName = it.key();
        const QString animationName = it.value().toString();

        if (animationName.isEmpty()) {
            qWarning() << "CharacterPack: Event map entry has empty animation for event:" << eventName;
            return false;
        }

        m_eventMap[eventName] = animationName;
    }

    return true;
}

bool CharacterPack::parseEffectTriggers(const QJsonObject &triggers)
{
    for (auto it = triggers.begin(); it != triggers.end(); ++it) {
        const QString animationName = it.key();
        const QString effectName = it.value().toString();

        if (effectName.isEmpty()) {
            qWarning() << "CharacterPack: Effect trigger has empty effect for animation:" << animationName;
            return false;
        }

        m_effectTriggers[animationName] = effectName;
    }

    return true;
}

bool CharacterPack::loadAnimationsFromDefinitions(const QString &definitionsPath)
{
    QFile file(definitionsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "CharacterPack: Failed to open definitions file:" << definitionsPath;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "CharacterPack: definitions parse error:" << error.errorString();
        return false;
    }

    if (!doc.isArray()) {
        qWarning() << "CharacterPack: definitions is not an array";
        return false;
    }

    // Parse original animations.json format
    // Each entry: {"Name": "...", "Frames": [{"Duration": ..., "ImagesOffsets": {"Column": ..., "Row": ...}}]}
    const QJsonArray anims = doc.array();
    for (const QJsonValue &val : anims) {
        QJsonObject animObj = val.toObject();
        const QString name = animObj["Name"].toString();

        AnimationDef def;
        def.name = name;
        def.type = EngineType::SpriteSheet;
        def.loop = false;  // Original format doesn't specify loop

        QJsonArray frames = animObj["Frames"].toArray();
        for (const QJsonValue &fval : frames) {
            QJsonObject frameObj = fval.toObject();
            QJsonObject offsets = frameObj["ImagesOffsets"].toObject();

            // Some frames have null/empty offsets (e.g., blank transition frames)
            if (offsets.isEmpty() || !offsets.contains("Column")) {
                offsets["Column"] = 0;
                offsets["Row"] = 0;
            }

            int col = offsets["Column"].toInt();
            int row = offsets["Row"].toInt();
            int duration = frameObj["Duration"].toInt();

            FrameDef frame;
            frame.col = col;
            frame.row = row;
            frame.durationMs = duration;
            def.frames.append(frame);
            def.totalDurationMs += duration;
        }

        m_animations.insert(name, def);
    }

    qDebug() << "CharacterPack: Loaded" << m_animations.size() << "animations from definitions:" << definitionsPath;
    return true;
}

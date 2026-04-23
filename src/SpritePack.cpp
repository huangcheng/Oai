#include "SpritePack.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QTemporaryDir>

bool SpritePack::loadFromDirectory(const QString &packDir)
{
    QDir dir(packDir);
    if (!dir.exists()) {
        qWarning() << "SpritePack: Directory does not exist:" << packDir;
        return false;
    }

    // Check for manifest.json
    const QString manifestPath = dir.absoluteFilePath("manifest.json");
    if (!QFile::exists(manifestPath)) {
        qWarning() << "SpritePack: No manifest.json found in:" << packDir;
        return false;
    }

    // Read and parse manifest
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qWarning() << "SpritePack: Failed to open manifest.json:" << manifestPath;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll(), &error);
    manifestFile.close();

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "SpritePack: manifest.json parse error:" << error.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "SpritePack: manifest.json is not an object";
        return false;
    }

    // Parse manifest
    m_rootPath = dir.absolutePath();
    if (!parseManifest(doc.object())) {
        qWarning() << "SpritePack: Failed to parse manifest";
        return false;
    }

    m_valid = true;
    qDebug() << "SpritePack: Loaded pack" << m_metadata.name << "from" << m_rootPath;
    return true;
}

bool SpritePack::loadFromArchive(const QString &archivePath, const QString &extractDir)
{
    // TODO: Implement ZIP extraction using QZipReader or miniz
    // For now, log a warning and return false
    qWarning() << "SpritePack: Archive loading not yet implemented:" << archivePath;
    return false;
}

const SpritePack::AnimationDef *SpritePack::animation(const QString &name) const
{
    auto it = m_animations.find(name);
    if (it != m_animations.end()) {
        return &it.value();
    }
    return nullptr;
}

QString SpritePack::assetPath(const QString &relativePath) const
{
    if (m_rootPath.isEmpty()) {
        return QString();
    }
    return QDir(m_rootPath).absoluteFilePath(relativePath);
}

QString SpritePack::lottieAnimationPath(const QString &animationName) const
{
    const AnimationDef *anim = animation(animationName);
    if (!anim || anim->type != EngineType::Lottie) {
        return QString();
    }
    return assetPath(anim->lottieFile);
}

QString SpritePack::effectPath(const QString &effectName) const
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

QString SpritePack::soundPath(const QString &soundName) const
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

QStringList SpritePack::availableLottieAnimations() const
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

QStringList SpritePack::availableEffects() const
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

QStringList SpritePack::availableSounds() const
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

bool SpritePack::parseManifest(const QJsonObject &manifest)
{
    // Parse format version
    m_metadata.formatVersion = manifest.value("formatVersion").toString();
    if (m_metadata.formatVersion != "1.0.0") {
        qWarning() << "SpritePack: Unsupported format version:" << m_metadata.formatVersion;
        return false;
    }

    // Parse required fields
    m_metadata.id = manifest.value("id").toString();
    m_metadata.name = manifest.value("name").toString();
    m_metadata.author = manifest.value("author").toString();
    m_metadata.version = manifest.value("version").toString();

    if (m_metadata.id.isEmpty() || m_metadata.name.isEmpty() || 
        m_metadata.author.isEmpty() || m_metadata.version.isEmpty()) {
        qWarning() << "SpritePack: Missing required metadata fields";
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
        qWarning() << "SpritePack: Failed to parse character configuration";
        return false;
    }

    return true;
}

bool SpritePack::parseCharacter(const QJsonObject &character)
{
    // Parse engine type
    const QString typeStr = character.value("type").toString();
    if (typeStr == "lottie") {
        m_characterConfig.engineType = EngineType::Lottie;
    } else if (typeStr == "spriteSheet") {
        m_characterConfig.engineType = EngineType::SpriteSheet;
    } else {
        qWarning() << "SpritePack: Unknown character type:" << typeStr;
        return false;
    }

    // Parse type-specific configuration
    if (m_characterConfig.engineType == EngineType::Lottie) {
        m_characterConfig.animDirectory = character.value("directory").toString("character");
    } else {
        m_characterConfig.spriteSheet = character.value("spriteSheet").toString();
        m_characterConfig.frameWidth = character.value("frameWidth").toInt();
        m_characterConfig.frameHeight = character.value("frameHeight").toInt();
        m_characterConfig.definitions = character.value("definitions").toString();

        if (m_characterConfig.spriteSheet.isEmpty() || 
            m_characterConfig.frameWidth <= 0 || 
            m_characterConfig.frameHeight <= 0) {
            qWarning() << "SpritePack: Invalid sprite sheet configuration";
            return false;
        }
    }

    // Parse animations (from definitions file or inline)
    if (!m_characterConfig.definitions.isEmpty()) {
        // Load animations from external definitions file (original animations.json format)
        const QString definitionsPath = assetPath(m_characterConfig.definitions);
        if (!loadAnimationsFromDefinitions(definitionsPath)) {
            qWarning() << "SpritePack: Failed to load animations from definitions:" << definitionsPath;
            return false;
        }
    } else {
        // Load animations from inline definitions
        const QJsonObject animationsObj = character.value("animations").toObject();
        if (!parseAnimations(animationsObj)) {
            qWarning() << "SpritePack: failed to parse animations";
            return false;
        }
    }

    // Parse idle pool
    const QJsonArray idlePoolArray = character.value("idlePool").toArray();
    if (!parseIdlePool(idlePoolArray)) {
        qWarning() << "SpritePack: Failed to parse idle pool";
        return false;
    }

    // Parse event map
    const QJsonObject eventMapObj = character.value("eventMap").toObject();
    if (!parseEventMap(eventMapObj)) {
        qWarning() << "SpritePack: Failed to parse event map";
        return false;
    }

    // Parse effect triggers
    const QJsonObject effectTriggersObj = character.value("effectTriggers").toObject();
    if (!parseEffectTriggers(effectTriggersObj)) {
        qWarning() << "SpritePack: Failed to parse effect triggers";
        return false;
    }

    return true;
}

bool SpritePack::parseAnimations(const QJsonObject &animations)
{
    for (auto it = animations.begin(); it != animations.end(); ++it) {
        const QString name = it.key();
        const QJsonObject animObj = it.value().toObject();

        AnimationDef anim;
        anim.name = name;

        if (!parseAnimationDef(name, animObj, anim)) {
            qWarning() << "SpritePack: Failed to parse animation:" << name;
            return false;
        }

        m_animations[name] = anim;
    }

    return true;
}

bool SpritePack::parseAnimationDef(const QString &name, const QJsonObject &def, AnimationDef &out)
{
    // Parse type
    const QString typeStr = def.value("type").toString();
    if (typeStr == "lottie") {
        out.type = EngineType::Lottie;
        out.lottieFile = def.value("file").toString();

        if (out.lottieFile.isEmpty()) {
            qWarning() << "SpritePack: Lottie animation missing 'file' field:" << name;
            return false;
        }
    } else if (typeStr == "sprite") {
        out.type = EngineType::SpriteSheet;

        const QJsonArray framesArray = def.value("frames").toArray();
        if (!parseFrames(framesArray, out.frames)) {
            qWarning() << "SpritePack: Failed to parse frames for animation:" << name;
            return false;
        }

        if (out.frames.isEmpty()) {
            qWarning() << "SpritePack: Sprite animation has no frames:" << name;
            return false;
        }
    } else {
        qWarning() << "SpritePack: Unknown animation type:" << typeStr << "for" << name;
        return false;
    }

    // Parse optional fields
    out.loop = def.value("loop").toBool(false);
    out.highPriority = def.value("priority").toString() == "high";
    out.effect = def.value("effect").toString();
    out.sound = def.value("sound").toString();

    return true;
}

bool SpritePack::parseFrames(const QJsonArray &frames, QVector<FrameDef> &out)
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
            qWarning() << "SpritePack: Frame missing position (col/row or x/y/w/h)";
            return false;
        }

        if (frame.durationMs <= 0) {
            qWarning() << "SpritePack: Frame has invalid duration:" << frame.durationMs;
            return false;
        }

        out.append(frame);
    }

    return true;
}

bool SpritePack::parseIdlePool(const QJsonArray &pool)
{
    for (const QJsonValue &entryVal : pool) {
        const QJsonObject entryObj = entryVal.toObject();
        IdleEntry entry;

        entry.animationName = entryObj.value("name").toString();
        entry.weight = entryObj.value("weight").toInt(1);

        if (entry.animationName.isEmpty()) {
            qWarning() << "SpritePack: Idle pool entry missing 'name'";
            return false;
        }

        if (entry.weight <= 0) {
            qWarning() << "SpritePack: Idle pool entry has invalid weight:" << entry.weight;
            return false;
        }

        m_idlePool.append(entry);
    }

    return true;
}

bool SpritePack::parseEventMap(const QJsonObject &map)
{
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QString eventName = it.key();
        const QString animationName = it.value().toString();

        if (animationName.isEmpty()) {
            qWarning() << "SpritePack: Event map entry has empty animation for event:" << eventName;
            return false;
        }

        m_eventMap[eventName] = animationName;
    }

    return true;
}

bool SpritePack::parseEffectTriggers(const QJsonObject &triggers)
{
    for (auto it = triggers.begin(); it != triggers.end(); ++it) {
        const QString animationName = it.key();
        const QString effectName = it.value().toString();

        if (effectName.isEmpty()) {
            qWarning() << "SpritePack: Effect trigger has empty effect for animation:" << animationName;
            return false;
        }

        m_effectTriggers[animationName] = effectName;
    }

    return true;
}

bool SpritePack::loadAnimationsFromDefinitions(const QString &definitionsPath)
{
    QFile file(definitionsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "SpritePack: Failed to open definitions file:" << definitionsPath;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "SpritePack: definitions parse error:" << error.errorString();
        return false;
    }

    if (!doc.isArray()) {
        qWarning() << "SpritePack: definitions is not an array";
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

    qDebug() << "SpritePack: Loaded" << m_animations.size() << "animations from definitions:" << definitionsPath;
    return true;
}

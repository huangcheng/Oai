#include "CharacterPackManager.h"
#include "CharacterPack.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>

#include "../thirdparty/miniz/miniz.h"

CharacterPackManager::CharacterPackManager(QObject *parent)
    : QObject(parent)
{
    m_hotReloadTimer = new QTimer(this);
    m_hotReloadTimer->setSingleShot(true);
    m_hotReloadTimer->setInterval(500);  // 500ms debounce
    connect(m_hotReloadTimer, &QTimer::timeout, this, &CharacterPackManager::onHotReloadTimer);
}

CharacterPackManager::~CharacterPackManager()
{
    cleanupFileWatcher();
    qDeleteAll(m_loadedPacks);
}

void CharacterPackManager::initialize(const QString &builtInDir, const QString &userDir,
                                       const QString &preferredId)
{
    m_builtInDir = builtInDir;
    m_userDir = userDir;

    // Ensure user directory exists
    QDir().mkpath(m_userDir);

    // Discover all packs (built-in stay in app bundle, user packs in config dir)
    discoverPacks();

    // Setup file watcher for hot-reload
    setupFileWatcher();

    // Load default pack: preferred ID from config (if still installed),
    // otherwise fall back to the alphabetically-first available pack.
    if (!m_packs.isEmpty()) {
        QString defaultPackId = preferredId;
        if (defaultPackId.isEmpty() || !m_packs.contains(defaultPackId)) {
            defaultPackId = m_packs.firstKey();
        }
        switchPack(defaultPackId);
    } else {
        qWarning() << "CharacterPackManager: No sprite packs found!";
    }
}

QVector<CharacterPackManager::PackInfo> CharacterPackManager::availablePacks() const
{
    return m_packs.values().toVector();
}

QString CharacterPackManager::PackInfo::displayName(const QString &localeCode) const
{
    if (!localeCode.isEmpty()) {
        if (auto it = nameLocalized.constFind(localeCode); it != nameLocalized.constEnd()) {
            return it.value();
        }
        const QString lang = localeCode.section('_', 0, 0);
        for (auto it = nameLocalized.constBegin(); it != nameLocalized.constEnd(); ++it) {
            if (it.key().section('_', 0, 0) == lang) {
                return it.value();
            }
        }
    }
    return name;
}

bool CharacterPackManager::switchPack(const QString &packId)
{
    qDebug() << "[SWITCH] Attempting switch to packId:" << packId
             << "m_activePackId current:" << m_activePackId
             << "available packs count:" << m_packs.size();
    if (!m_packs.contains(packId)) {
        qWarning() << "CharacterPackManager: Pack not found in m_packs:" << packId;
        qWarning() << "  Available pack IDs:";
        for (const auto &key : m_packs.keys()) {
            qWarning() << "    -" << key;
        }
        return false;
    }

    if (packId == m_activePackId && m_activePack) {
        qDebug() << "CharacterPackManager: Pack already active:" << packId;
        return true;
    }

    const PackInfo &info = m_packs[packId];
    qDebug() << "  Pack info: name=" << info.name << "path=" << info.path;

    // Load pack if not already loaded
    if (!m_loadedPacks.contains(packId)) {
        qDebug() << "  Loading pack from path:" << info.path;
        CharacterPack *pack = createAndLoadPack(info.path);
        if (!pack) {
            qWarning() << "CharacterPackManager: createAndLoadPack returned nullptr for:" << packId;
            return false;
        }
        qDebug() << "  Pack loaded successfully, isValid:" << pack->isValid()
                 << "engineType:" << static_cast<int>(pack->characterConfig().engineType);
        m_loadedPacks[packId] = pack;
    }

    // Switch active pack
    m_activePackId = packId;
    m_activePack = m_loadedPacks[packId];

    qDebug() << "CharacterPackManager: Switched to pack:" << info.name;
    emit activePackChanged(m_activePack);

    return true;
}

bool CharacterPackManager::installPack(const QString &archivePath)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archivePath.toUtf8().constData(), 0)) {
        qWarning() << "CharacterPackManager: Failed to open archive:" << archivePath;
        return false;
    }

    // First pass: read manifest.json to get the pack ID
    int manifestIdx = mz_zip_reader_locate_file(&zip, "manifest.json", nullptr, 0);
    if (manifestIdx < 0) {
        qWarning() << "CharacterPackManager: No manifest.json in archive:" << archivePath;
        mz_zip_reader_end(&zip);
        return false;
    }

    size_t manifestSize = 0;
    void *manifestData = mz_zip_reader_extract_to_heap(&zip, manifestIdx, &manifestSize, 0);
    if (!manifestData) {
        qWarning() << "CharacterPackManager: Failed to read manifest.json from:" << archivePath;
        mz_zip_reader_end(&zip);
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(static_cast<const char *>(manifestData),
                                                           static_cast<int>(manifestSize)));
    mz_free(manifestData);

    if (!doc.isObject()) {
        qWarning() << "CharacterPackManager: Invalid manifest.json in:" << archivePath;
        mz_zip_reader_end(&zip);
        return false;
    }

    QString packId = doc.object().value("id").toString();
    if (packId.isEmpty()) {
        qWarning() << "CharacterPackManager: Pack manifest missing 'id' in:" << archivePath;
        mz_zip_reader_end(&zip);
        return false;
    }

    // Extract to a sibling .tmp directory first, then atomically rename on
    // success. Without this, an extraction failure mid-way (disk full,
    // permission denied, corrupt entry) leaves a half-installed pack at the
    // final path that next launch will try to load and warn about.
    QString installDir = m_userDir + "/" + packId;
    QString stagingDir = installDir + ".tmp";

    // Wipe any leftover staging from a prior interrupted install.
    QDir(stagingDir).removeRecursively();
    if (!QDir().mkpath(stagingDir)) {
        qWarning() << "CharacterPackManager: Failed to create staging dir:" << stagingDir;
        mz_zip_reader_end(&zip);
        return false;
    }

    int numFiles = static_cast<int>(mz_zip_reader_get_num_files(&zip));
    bool extractOk = true;
    for (int i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;

        QString entryName = QString::fromUtf8(stat.m_filename);
        QString destPath = stagingDir + "/" + entryName;

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            QDir().mkpath(destPath);
        } else {
            // Ensure parent directory exists
            QDir().mkpath(QFileInfo(destPath).absolutePath());
            if (!mz_zip_reader_extract_to_file(&zip, i, destPath.toUtf8().constData(), 0)) {
                qWarning() << "CharacterPackManager: Failed to extract:" << entryName;
                extractOk = false;
                break;
            }
        }
    }

    mz_zip_reader_end(&zip);

    if (!extractOk) {
        QDir(stagingDir).removeRecursively();
        return false;
    }

    // Atomically replace the existing install (if any) with the staged copy.
    if (QDir(installDir).exists() && !QDir(installDir).removeRecursively()) {
        qWarning() << "CharacterPackManager: Failed to remove existing install:" << installDir;
        QDir(stagingDir).removeRecursively();
        return false;
    }
    if (!QDir().rename(stagingDir, installDir)) {
        qWarning() << "CharacterPackManager: Failed to rename staging dir to:" << installDir;
        QDir(stagingDir).removeRecursively();
        return false;
    }

    qDebug() << "CharacterPackManager: Installed pack" << packId << "to" << installDir;

    // Refresh pack list so UI updates immediately
    discoverPacks();
    emit packListChanged();

    return true;
}

bool CharacterPackManager::uninstallPack(const QString &packId)
{
    if (!m_packs.contains(packId)) {
        qWarning() << "CharacterPackManager: Pack not found:" << packId;
        return false;
    }

    const PackInfo &info = m_packs[packId];

    // Only user packs can be uninstalled
    if (info.source != PackSource::User) {
        qWarning() << "CharacterPackManager: Cannot uninstall built-in pack:" << packId;
        return false;
    }

    // Cannot uninstall the currently active pack
    if (packId == m_activePackId) {
        qWarning() << "CharacterPackManager: Cannot uninstall active pack:" << packId;
        return false;
    }

    QFileInfo pathInfo(info.path);
    if (pathInfo.isFile()) {
        if (!QFile::remove(info.path)) {
            qWarning() << "CharacterPackManager: Failed to remove pack file:" << info.path;
            return false;
        }
    } else if (pathInfo.isDir()) {
        QDir packDir(info.path);
        if (!packDir.removeRecursively()) {
            qWarning() << "CharacterPackManager: Failed to remove pack directory:" << info.path;
            return false;
        }
    }

    // Remove from loaded packs
    if (m_loadedPacks.contains(packId)) {
        delete m_loadedPacks[packId];
        m_loadedPacks.remove(packId);
    }

    // Remove from packs map
    m_packs.remove(packId);

    qDebug() << "CharacterPackManager: Uninstalled pack:" << packId;
    emit packListChanged();

    return true;
}

bool CharacterPackManager::isPackInstalled(const QString &packId) const
{
    return m_packs.contains(packId);
}

CharacterPackManager::PackInfo CharacterPackManager::packInfo(const QString &packId) const
{
    return m_packs.value(packId);
}

void CharacterPackManager::setHotReloadEnabled(bool enabled)
{
    m_hotReloadEnabled = enabled;
    if (enabled) {
        setupFileWatcher();
    } else {
        cleanupFileWatcher();
    }
}

void CharacterPackManager::reloadCurrentPack()
{
    if (m_activePackId.isEmpty() || !m_activePack) {
        return;
    }

    const PackInfo &info = m_packs[m_activePackId];

    // Reload pack
    CharacterPack *newPack = createAndLoadPack(info.path);
    if (!newPack) {
        qWarning() << "CharacterPackManager: Failed to reload pack:" << m_activePackId;
        return;
    }

    // Replace loaded pack
    delete m_loadedPacks[m_activePackId];
    m_loadedPacks[m_activePackId] = newPack;
    m_activePack = newPack;

    qDebug() << "CharacterPackManager: Reloaded pack:" << info.name;
    emit packReloaded(m_activePack);
}

void CharacterPackManager::discoverPacks()
{
    m_packs.clear();

    auto scanDir = [&](const QString &dirPath, PackSource source) {
        QDir dir(dirPath);
        if (!dir.exists()) return;

        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            loadPackFromDirectory(entry.absoluteFilePath(), source);
        }

        const QStringList codexFilters = {"*.codex-pet", "*.codex-pet.zip"};
        const QFileInfoList codexFiles = dir.entryInfoList(codexFilters, QDir::Files);
        for (const QFileInfo &entry : codexFiles) {
            loadPackFromCodexPet(entry.absoluteFilePath(), source);
        }
    };

    // Scan user dir first, then built-in. This ensures that any stale
    // built-in pack copies left in the user directory get overridden by
    // the real built-in entry, keeping their source as BuiltIn so they
    // are filtered out of the Models Management dialog.
    if (!m_userDir.isEmpty()) {
        scanDir(m_userDir, PackSource::User);
    }

    if (!m_builtInDir.isEmpty()) {
        scanDir(m_builtInDir, PackSource::BuiltIn);
    }

    qDebug() << "CharacterPackManager: Discovered" << m_packs.size() << "packs:";
    for (auto it = m_packs.constBegin(); it != m_packs.constEnd(); ++it) {
        qDebug() << "  -" << it.key()
                 << "source=" << (it->source == PackSource::BuiltIn ? "BuiltIn" : "User")
                 << "path=" << it->path;
    }
    emit packListChanged();
}

QString CharacterPackManager::extractPackIdFromOpk(const QString &opkPath)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, opkPath.toUtf8().constData(), 0)) {
        return QString();
    }

    int manifestIdx = mz_zip_reader_locate_file(&zip, "manifest.json", nullptr, 0);
    if (manifestIdx < 0) {
        mz_zip_reader_end(&zip);
        return QString();
    }

    size_t manifestSize = 0;
    void *manifestData = mz_zip_reader_extract_to_heap(&zip, manifestIdx, &manifestSize, 0);
    mz_zip_reader_end(&zip);

    if (!manifestData) {
        return QString();
    }

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(static_cast<const char *>(manifestData),
                                                           static_cast<int>(manifestSize)));
    mz_free(manifestData);

    if (!doc.isObject()) {
        return QString();
    }

    return doc.object().value("id").toString();
}

void CharacterPackManager::loadPackFromDirectory(const QString &packDir, PackSource source)
{
    // Check for manifest.json
    const QString manifestPath = QDir(packDir).absoluteFilePath("manifest.json");
    if (!QFile::exists(manifestPath)) {
        return;
    }

    // Read manifest to get pack ID
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qWarning() << "CharacterPackManager: Failed to open manifest:" << manifestPath;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();

    if (!doc.isObject()) {
        qWarning() << "CharacterPackManager: Invalid manifest:" << manifestPath;
        return;
    }

    const QJsonObject manifest = doc.object();
    const QString packId = manifest.value("id").toString();

    if (packId.isEmpty()) {
        qWarning() << "CharacterPackManager: Pack missing ID:" << manifestPath;
        return;
    }

    // Create pack info
    PackInfo info;
    info.id = packId;
    info.name = manifest.value("name").toString();
    const QJsonObject locNames = manifest.value("nameLocalized").toObject();
    for (auto it = locNames.begin(); it != locNames.end(); ++it) {
        const QString locale = it.key();
        const QString value = it.value().toString();
        if (!locale.isEmpty() && !value.isEmpty()) {
            info.nameLocalized.insert(locale, value);
        }
    }
    info.author = manifest.value("author").toString();
    info.version = manifest.value("version").toString();
    info.description = manifest.value("description").toString();
    info.preview = manifest.value("preview").toString();
    info.path = packDir;
    info.category = manifest.value("category").toString();
    // Legacy fallback: pre-`category` Azur Lane manifests can be detected
    // by the import-script-stamped author string.
    if (info.category.isEmpty()) {
        info.category = info.author.startsWith(QStringLiteral("Imported from github.com/"))
                            ? QStringLiteral("azur_lane")
                            : QStringLiteral("originals");
    }
    info.source = source;

    // Add to packs map (user packs override built-in with same ID)
    m_packs[packId] = info;

    qDebug() << "CharacterPackManager: Found pack:" << info.name << "(" << packId << ")";
}

void CharacterPackManager::loadPackFromCodexPet(const QString &archivePath, PackSource source)
{
    PackInfo info;
    if (!extractCodexPetInfo(archivePath, info)) {
        return;
    }

    info.path = archivePath;
    info.source = source;
    info.category = "codex";

    m_packs[info.id] = info;
    qDebug() << "CharacterPackManager: Found Codex pet:" << info.name << "(" << info.id << ")";
}

bool CharacterPackManager::extractCodexPetInfo(const QString &archivePath, PackInfo &outInfo)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archivePath.toUtf8().constData(), 0)) {
        return false;
    }

    int petJsonIdx = mz_zip_reader_locate_file(&zip, "pet.json", nullptr, 0);
    if (petJsonIdx < 0) {
        mz_zip_reader_end(&zip);
        return false;
    }

    size_t petJsonSize = 0;
    void *petJsonData = mz_zip_reader_extract_to_heap(&zip, petJsonIdx, &petJsonSize, 0);
    mz_zip_reader_end(&zip);

    if (!petJsonData) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray(static_cast<const char *>(petJsonData), static_cast<int>(petJsonSize)));
    mz_free(petJsonData);

    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject obj = doc.object();
    outInfo.id = obj.value("id").toString();
    outInfo.name = obj.value("displayName").toString();
    outInfo.description = obj.value("description").toString();
    outInfo.author = "codex";
    outInfo.version = "1.0.0";

    if (outInfo.id.isEmpty() || outInfo.name.isEmpty()) {
        return false;
    }

    return true;
}

CharacterPack *CharacterPackManager::createAndLoadPack(const QString &packPath)
{
    CharacterPack *pack = new CharacterPack();

    if (packPath.endsWith(".codex-pet", Qt::CaseInsensitive) ||
        packPath.endsWith(".codex-pet.zip", Qt::CaseInsensitive)) {
        if (!pack->loadFromCodexPet(packPath)) {
            qWarning() << "CharacterPackManager: Failed to load Codex pet from:" << packPath;
            delete pack;
            return nullptr;
        }
        return pack;
    }

    if (!pack->loadFromDirectory(packPath)) {
        qWarning() << "CharacterPackManager: Failed to load pack from:" << packPath;
        delete pack;
        return nullptr;
    }
    return pack;
}

void CharacterPackManager::setupFileWatcher()
{
    if (m_fileWatcher) {
        return;  // Already setup
    }

    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &CharacterPackManager::onDirectoryChanged);

    // Watch user packs directory
    if (!m_userDir.isEmpty() && QDir(m_userDir).exists()) {
        m_fileWatcher->addPath(m_userDir);
    }
}

void CharacterPackManager::cleanupFileWatcher()
{
    if (m_fileWatcher) {
        delete m_fileWatcher;
        m_fileWatcher = nullptr;
    }
}

void CharacterPackManager::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path);

    if (!m_hotReloadEnabled) {
        return;
    }

    // Debounce: restart timer
    m_hotReloadPending = true;
    m_hotReloadTimer->start();
}

void CharacterPackManager::onHotReloadTimer()
{
    if (!m_hotReloadPending) {
        return;
    }

    m_hotReloadPending = false;

    // Re-discover packs
    discoverPacks();

    // If active pack is still available, reload it
    if (m_packs.contains(m_activePackId)) {
        reloadCurrentPack();
    } else {
        // Active pack was removed, switch to first available
        if (!m_packs.isEmpty()) {
            switchPack(m_packs.firstKey());
        }
    }

    emit packListChanged();
}

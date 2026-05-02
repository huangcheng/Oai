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

    // Auto-install built-in packs if user packs directory is empty
    autoInstallBuiltInPacks();

    // Discover all packs
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
    if (!m_packs.contains(packId)) {
        qWarning() << "CharacterPackManager: Pack not found:" << packId;
        return false;
    }

    if (packId == m_activePackId && m_activePack) {
        // Already active
        return true;
    }

    const PackInfo &info = m_packs[packId];

    // Load pack if not already loaded
    if (!m_loadedPacks.contains(packId)) {
        CharacterPack *pack = createAndLoadPack(info.path);
        if (!pack) {
            qWarning() << "CharacterPackManager: Failed to load pack:" << packId;
            return false;
        }
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

    // If this is the active pack, switch to another
    if (packId == m_activePackId) {
        // Find another pack to switch to
        QString newPackId;
        for (auto it = m_packs.begin(); it != m_packs.end(); ++it) {
            if (it.key() != packId) {
                newPackId = it.key();
                break;
            }
        }

        if (newPackId.isEmpty()) {
            qWarning() << "CharacterPackManager: Cannot uninstall last remaining pack";
            return false;
        }

        switchPack(newPackId);
    }

    // Remove pack directory
    QDir packDir(info.path);
    if (packDir.exists()) {
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

    // Scan built-in packs from filesystem
    if (!m_builtInDir.isEmpty()) {
        QDir builtInDir(m_builtInDir);
        if (builtInDir.exists()) {
            const QFileInfoList entries = builtInDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &entry : entries) {
                loadPackFromDirectory(entry.absoluteFilePath(), PackSource::BuiltIn);
            }
        }
    }

    // Scan user packs directory
    if (!m_userDir.isEmpty()) {
        QDir userDir(m_userDir);
        if (userDir.exists()) {
            const QFileInfoList entries = userDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &entry : entries) {
                loadPackFromDirectory(entry.absoluteFilePath(), PackSource::User);
            }
        }
    }

    qDebug() << "CharacterPackManager: Discovered" << m_packs.size() << "packs";
}

void CharacterPackManager::autoInstallBuiltInPacks()
{
    if (m_userDir.isEmpty()) {
        return;
    }

    // Look for .opk files in multiple locations
    QStringList searchPaths;
    
    // 1. Next to the executable (works for all platforms)
    QString appDir = QCoreApplication::applicationDirPath();
    searchPaths.append(appDir + "/packs");
    
    // 2. Inside .app bundle on macOS
    #ifdef Q_OS_MAC
    QDir bundleDir(appDir);
    if (bundleDir.cdUp() && bundleDir.cd("Resources")) {
        searchPaths.append(bundleDir.absoluteFilePath("packs"));
    }
    #endif
    
    // 3. In the assets directory (for development)
    QDir assetsDir(appDir);
    if (assetsDir.cdUp() && assetsDir.cd("assets")) {
        searchPaths.append(assetsDir.absoluteFilePath("packs"));
    }

    // Search all paths for .opk files
    for (const QString &packsPath : searchPaths) {
        QDir packsDir(packsPath);
        if (!packsDir.exists()) {
            continue;
        }

        const QFileInfoList opkFiles = packsDir.entryInfoList({"*.opk"}, QDir::Files);
        for (const QFileInfo &opkFile : opkFiles) {
            // Extract pack ID from the .opk file
            QString packId = extractPackIdFromOpk(opkFile.absoluteFilePath());
            if (packId.isEmpty()) {
                continue;
            }

            // Check if already installed
            QString installDir = m_userDir + "/" + packId;
            if (QDir(installDir).exists()) {
                qDebug() << "CharacterPackManager: Pack already installed:" << packId;
                continue;
            }

            // Install the pack
            qDebug() << "CharacterPackManager: Auto-installing pack:" << opkFile.fileName();
            if (installPack(opkFile.absoluteFilePath())) {
                qDebug() << "CharacterPackManager: Successfully installed:" << packId;
            } else {
                qWarning() << "CharacterPackManager: failed to auto-install:" << opkFile.fileName();
            }
        }
    }
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

CharacterPack *CharacterPackManager::createAndLoadPack(const QString &packDir)
{
    CharacterPack *pack = new CharacterPack();
    if (!pack->loadFromDirectory(packDir)) {
        qWarning() << "CharacterPackManager: Failed to load pack from:" << packDir;
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

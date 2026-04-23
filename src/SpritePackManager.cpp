#include "SpritePackManager.h"
#include "SpritePack.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>

SpritePackManager::SpritePackManager(QObject *parent)
    : QObject(parent)
{
    m_hotReloadTimer = new QTimer(this);
    m_hotReloadTimer->setSingleShot(true);
    m_hotReloadTimer->setInterval(500);  // 500ms debounce
    connect(m_hotReloadTimer, &QTimer::timeout, this, &SpritePackManager::onHotReloadTimer);
}

SpritePackManager::~SpritePackManager()
{
    cleanupFileWatcher();
    qDeleteAll(m_loadedPacks);
}

void SpritePackManager::initialize(const QString &builtInDir, const QString &userDir)
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

    // Load default pack (first available, or fallback)
    if (!m_packs.isEmpty()) {
        // Try to load 'clippy' as default, or first available
        QString defaultPackId = "com.oraipet.clippy";
        if (!m_packs.contains(defaultPackId)) {
            defaultPackId = m_packs.firstKey();
        }
        switchPack(defaultPackId);
    } else {
        qWarning() << "SpritePackManager: No sprite packs found!";
    }
}

QVector<SpritePackManager::PackInfo> SpritePackManager::availablePacks() const
{
    return m_packs.values().toVector();
}

bool SpritePackManager::switchPack(const QString &packId)
{
    if (!m_packs.contains(packId)) {
        qWarning() << "SpritePackManager: Pack not found:" << packId;
        return false;
    }

    if (packId == m_activePackId && m_activePack) {
        // Already active
        return true;
    }

    const PackInfo &info = m_packs[packId];

    // Load pack if not already loaded
    if (!m_loadedPacks.contains(packId)) {
        SpritePack *pack = createAndLoadPack(info.path);
        if (!pack) {
            qWarning() << "SpritePackManager: Failed to load pack:" << packId;
            return false;
        }
        m_loadedPacks[packId] = pack;
    }

    // Switch active pack
    m_activePackId = packId;
    m_activePack = m_loadedPacks[packId];

    qDebug() << "SpritePackManager: Switched to pack:" << info.name;
    emit activePackChanged(m_activePack);

    return true;
}

bool SpritePackManager::installPack(const QString &archivePath)
{
    // TODO: Implement .opk archive extraction
    qWarning() << "SpritePackManager: Pack installation not yet implemented:" << archivePath;
    return false;
}

bool SpritePackManager::uninstallPack(const QString &packId)
{
    if (!m_packs.contains(packId)) {
        qWarning() << "SpritePackManager: Pack not found:" << packId;
        return false;
    }

    const PackInfo &info = m_packs[packId];

    // Only user packs can be uninstalled
    if (info.source != PackSource::User) {
        qWarning() << "SpritePackManager: Cannot uninstall built-in pack:" << packId;
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
            qWarning() << "SpritePackManager: Cannot uninstall last remaining pack";
            return false;
        }

        switchPack(newPackId);
    }

    // Remove pack directory
    QDir packDir(info.path);
    if (packDir.exists()) {
        if (!packDir.removeRecursively()) {
            qWarning() << "SpritePackManager: Failed to remove pack directory:" << info.path;
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

    qDebug() << "SpritePackManager: Uninstalled pack:" << packId;
    emit packListChanged();

    return true;
}

bool SpritePackManager::isPackInstalled(const QString &packId) const
{
    return m_packs.contains(packId);
}

SpritePackManager::PackInfo SpritePackManager::packInfo(const QString &packId) const
{
    return m_packs.value(packId);
}

void SpritePackManager::setHotReloadEnabled(bool enabled)
{
    m_hotReloadEnabled = enabled;
    if (enabled) {
        setupFileWatcher();
    } else {
        cleanupFileWatcher();
    }
}

void SpritePackManager::reloadCurrentPack()
{
    if (m_activePackId.isEmpty() || !m_activePack) {
        return;
    }

    const PackInfo &info = m_packs[m_activePackId];

    // Reload pack
    SpritePack *newPack = createAndLoadPack(info.path);
    if (!newPack) {
        qWarning() << "SpritePackManager: Failed to reload pack:" << m_activePackId;
        return;
    }

    // Replace loaded pack
    delete m_loadedPacks[m_activePackId];
    m_loadedPacks[m_activePackId] = newPack;
    m_activePack = newPack;

    qDebug() << "SpritePackManager: Reloaded pack:" << info.name;
    emit packReloaded(m_activePack);
}

void SpritePackManager::discoverPacks()
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

    qDebug() << "SpritePackManager: Discovered" << m_packs.size() << "packs";
}

void SpritePackManager::autoInstallBuiltInPacks()
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
                qDebug() << "SpritePackManager: Pack already installed:" << packId;
                continue;
            }

            // Install the pack
            qDebug() << "SpritePackManager: Auto-installing pack:" << opkFile.fileName();
            if (installPack(opkFile.absoluteFilePath())) {
                qDebug() << "SpritePackManager: Successfully installed:" << packId;
            } else {
                qWarning() << "SpritePackManager: failed to auto-install:" << opkFile.fileName();
            }
        }
    }
}

QString SpritePackManager::extractPackIdFromOpk(const QString &opkPath)
{
    // Open the .opk file (ZIP) and read manifest.json to get the pack ID
    // For now, use a simple approach - read the first .json file found
    // TODO: Implement proper ZIP reading
    Q_UNUSED(opkPath);
    return QString();
}

void SpritePackManager::loadPackFromDirectory(const QString &packDir, PackSource source)
{
    // Check for manifest.json
    const QString manifestPath = QDir(packDir).absoluteFilePath("manifest.json");
    if (!QFile::exists(manifestPath)) {
        return;
    }

    // Read manifest to get pack ID
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qWarning() << "SpritePackManager: Failed to open manifest:" << manifestPath;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();

    if (!doc.isObject()) {
        qWarning() << "SpritePackManager: Invalid manifest:" << manifestPath;
        return;
    }

    const QJsonObject manifest = doc.object();
    const QString packId = manifest.value("id").toString();

    if (packId.isEmpty()) {
        qWarning() << "SpritePackManager: Pack missing ID:" << manifestPath;
        return;
    }

    // Create pack info
    PackInfo info;
    info.id = packId;
    info.name = manifest.value("name").toString();
    info.author = manifest.value("author").toString();
    info.version = manifest.value("version").toString();
    info.description = manifest.value("description").toString();
    info.preview = manifest.value("preview").toString();
    info.path = packDir;
    info.source = source;

    // Add to packs map (user packs override built-in with same ID)
    m_packs[packId] = info;

    qDebug() << "SpritePackManager: Found pack:" << info.name << "(" << packId << ")";
}

SpritePack *SpritePackManager::createAndLoadPack(const QString &packDir)
{
    SpritePack *pack = new SpritePack();
    if (!pack->loadFromDirectory(packDir)) {
        qWarning() << "SpritePackManager: Failed to load pack from:" << packDir;
        delete pack;
        return nullptr;
    }
    return pack;
}

void SpritePackManager::setupFileWatcher()
{
    if (m_fileWatcher) {
        return;  // Already setup
    }

    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &SpritePackManager::onDirectoryChanged);

    // Watch user packs directory
    if (!m_userDir.isEmpty() && QDir(m_userDir).exists()) {
        m_fileWatcher->addPath(m_userDir);
    }
}

void SpritePackManager::cleanupFileWatcher()
{
    if (m_fileWatcher) {
        delete m_fileWatcher;
        m_fileWatcher = nullptr;
    }
}

void SpritePackManager::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path);

    if (!m_hotReloadEnabled) {
        return;
    }

    // Debounce: restart timer
    m_hotReloadPending = true;
    m_hotReloadTimer->start();
}

void SpritePackManager::onHotReloadTimer()
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

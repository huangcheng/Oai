#ifndef SPRITEPACKMANAGER_H
#define SPRITEPACKMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QFileSystemWatcher>
#include <QTimer>

class SpritePack;

/**
 * @brief Manages sprite pack discovery, loading, and switching.
 *
 * SpritePackManager scans multiple directories for sprite packs:
 * - Built-in packs (app directory)
 * - User packs (~/.config/Oai/packs/)
 *
 * Supports:
 * - Hot-reload via file system watching
 * - Pack switching without restart
 * - Fallback to default pack
 */
class SpritePackManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Pack source location
     */
    enum class PackSource {
        BuiltIn,  ///< App directory (read-only)
        User      ///< User config directory (read-write)
    };

    /**
     * @brief Pack information for discovery
     */
    struct PackInfo {
        QString id;               ///< Unique pack identifier
        QString name;             ///< Display name
        QString author;           ///< Creator name
        QString version;          ///< Pack version
        QString description;      ///< Short description
        QString preview;          ///< Path to preview image
        QString path;             ///< Path to pack directory
        PackSource source;        ///< Where pack is located
    };

    explicit SpritePackManager(QObject *parent = nullptr);
    ~SpritePackManager() override;

    /**
     * @brief Initialize the manager and discover packs
     * @param builtInDir Directory containing built-in packs
     * @param userDir Directory containing user packs
     */
    void initialize(const QString &builtInDir, const QString &userDir);

    /**
     * @brief Get list of all discovered packs
     */
    QVector<PackInfo> availablePacks() const;

    /**
     * @brief Get current active pack
     * @return Pointer to active pack, or nullptr if none loaded
     */
    SpritePack *activePack() const { return m_activePack; }

    /**
     * @brief Get active pack ID
     */
    QString activePackId() const { return m_activePackId; }

    /**
     * @brief Switch to a different pack
     * @param packId Pack identifier to switch to
     * @return true if switch was successful
     */
    bool switchPack(const QString &packId);

    /**
     * @brief Install a pack from .opk archive
     * @param archivePath Path to .opk file
     * @return true if installation was successful
     */
    bool installPack(const QString &archivePath);

    /**
     * @brief Uninstall a user pack
     * @param packId Pack identifier to uninstall
     * @return true if uninstallation was successful
     */
    bool uninstallPack(const QString &packId);

    /**
     * @brief Check if a pack is installed
     */
    bool isPackInstalled(const QString &packId) const;

    /**
     * @brief Get pack info by ID
     */
    PackInfo packInfo(const QString &packId) const;

    /**
     * @brief Enable/disable hot-reload
     */
    void setHotReloadEnabled(bool enabled);

    /**
     * @brief Force reload of current pack (for development)
     */
    void reloadCurrentPack();

signals:
    /**
     * @brief Emitted when active pack changes
     */
    void activePackChanged(SpritePack *pack);

    /**
     * @brief Emitted when pack list changes (install/uninstall)
     */
    void packListChanged();

    /**
     * @brief Emitted when pack is reloaded (hot-reload)
     */
    void packReloaded(SpritePack *pack);

private slots:
    void onDirectoryChanged(const QString &path);
    void onHotReloadTimer();

private:
    void discoverPacks();
    void loadPackFromDirectory(const QString &packDir, PackSource source);
    SpritePack *createAndLoadPack(const QString &packDir);
    void setupFileWatcher();
    void cleanupFileWatcher();
    void autoInstallBuiltInPacks();
    QString extractPackIdFromOpk(const QString &opkPath);

    QString m_builtInDir;
    QString m_userDir;
    QString m_activePackId;
    SpritePack *m_activePack = nullptr;

    QMap<QString, PackInfo> m_packs;
    QMap<QString, SpritePack *> m_loadedPacks;

    QFileSystemWatcher *m_fileWatcher = nullptr;
    QTimer *m_hotReloadTimer = nullptr;
    bool m_hotReloadEnabled = false;
    bool m_hotReloadPending = false;
};

#endif // SPRITEPACKMANAGER_H

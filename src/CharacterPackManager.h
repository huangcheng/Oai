#ifndef CHARACTERPACKMANAGER_H
#define CHARACTERPACKMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QFileSystemWatcher>
#include <QTimer>

class CharacterPack;

/**
 * @brief Manages sprite pack discovery, loading, and switching.
 *
 * CharacterPackManager scans multiple directories for sprite packs:
 * - Built-in packs (app directory)
 * - User packs (~/.config/Oai/packs/)
 *
 * Supports:
 * - Hot-reload via file system watching
 * - Pack switching without restart
 * - Fallback to default pack
 */
class CharacterPackManager : public QObject
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
        QString name;             ///< Display name (fallback / English)
        QMap<QString, QString> nameLocalized;  ///< Locale code → localized name
        QString author;           ///< Creator name
        QString version;          ///< Pack version
        QString description;      ///< Short description
        QString preview;          ///< Path to preview image
        QString path;             ///< Path to pack directory
        QString category;         ///< Source/franchise grouping (used by the menu partition)
        PackSource source;        ///< Where pack is located

        /**
         * @brief Resolve display name for a locale (exact → language → fallback to `name`).
         */
        QString displayName(const QString &localeCode) const;
    };

    explicit CharacterPackManager(QObject *parent = nullptr);
    ~CharacterPackManager() override;

    /**
     * @brief Initialize the manager and discover packs
     * @param builtInDir Directory containing built-in packs
     * @param userDir Directory containing user packs
     * @param preferredId Pack ID to activate if installed (empty = alphabetically first)
     */
    void initialize(const QString &builtInDir, const QString &userDir,
                    const QString &preferredId = QString());

    /**
     * @brief Get list of all discovered packs
     */
    QVector<PackInfo> availablePacks() const;

    /**
     * @brief Get current active pack
     * @return Pointer to active pack, or nullptr if none loaded
     */
    CharacterPack *activePack() const { return m_activePack; }

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
     * @brief Set the active locale for display-name resolution.
     *
     * Consumers (system tray, settings panel) call PackInfo::displayName()
     * with this code so a runtime language switch shows localized pack names
     * without re-parsing manifests.
     */
    void setActiveLocale(const QString &localeCode) { m_activeLocale = localeCode; }
    QString activeLocale() const { return m_activeLocale; }

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
    void activePackChanged(CharacterPack *pack);

    /**
     * @brief Emitted when pack list changes (install/uninstall)
     */
    void packListChanged();

    /**
     * @brief Emitted when pack is reloaded (hot-reload)
     */
    void packReloaded(CharacterPack *pack);

private slots:
    void onDirectoryChanged(const QString &path);
    void onHotReloadTimer();

private:
    void discoverPacks();
    void loadPackFromDirectory(const QString &packDir, PackSource source);
    CharacterPack *createAndLoadPack(const QString &packDir);
    void setupFileWatcher();
    void cleanupFileWatcher();
    void autoInstallBuiltInPacks();
    QString extractPackIdFromOpk(const QString &opkPath);

    QString m_builtInDir;
    QString m_userDir;
    QString m_activePackId;
    QString m_activeLocale;
    CharacterPack *m_activePack = nullptr;

    QMap<QString, PackInfo> m_packs;
    QMap<QString, CharacterPack *> m_loadedPacks;

    QFileSystemWatcher *m_fileWatcher = nullptr;
    QTimer *m_hotReloadTimer = nullptr;
    bool m_hotReloadEnabled = false;
    bool m_hotReloadPending = false;
};

#endif // CHARACTERPACKMANAGER_H

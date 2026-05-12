#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QObject>
#include <QSystemTrayIcon>

class QSystemTrayIcon;
class QMenu;
class QWidget;
class QAction;
class QActionGroup;
class CharacterPackManager;
class UpdateChecker;
class ConfigManager;
class PackManagerDialog;

class SystemTray : public QObject
{
    Q_OBJECT

public:
    explicit SystemTray(QWidget *mainWindow, ConfigManager *config, QObject *parent = nullptr);
    ~SystemTray() override;

    void show();
    void retranslateUi();

    // Set sprite pack manager for pack submenu
    void setCharacterPackManager(CharacterPackManager *manager);

    // Set update checker
    void setUpdateChecker(UpdateChecker *checker);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onPackActionTriggered();
    void onManageModelsClicked();
    void onUpdateAvailable(const QString &current, const QString &latest, const QString &url);
    void onNoUpdateAvailable(const QString &current);
    void onUpdateCheckFailed(const QString &error);

private:
    void setupMenu();
    void refreshPackMenu();

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QMenu *m_packMenu = nullptr;
    QActionGroup *m_packActionGroup = nullptr;
    QWidget *m_mainWindow = nullptr;
    QAction *m_toggleAction = nullptr;
    QAction *m_gamingModeAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_checkUpdateAction = nullptr;
    QAction *m_manageModelsAction = nullptr;
    CharacterPackManager *m_packManager = nullptr;
    UpdateChecker *m_updateChecker = nullptr;
    ConfigManager *m_config = nullptr;
    PackManagerDialog *m_packManagerDialog = nullptr;
};

#endif // SYSTEMTRAY_H

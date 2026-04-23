#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QObject>
#include <QSystemTrayIcon>

class QSystemTrayIcon;
class QMenu;
class QWidget;
class QAction;
class SpritePackManager;
class UpdateChecker;

class SystemTray : public QObject
{
    Q_OBJECT

public:
    explicit SystemTray(QWidget *mainWindow, QObject *parent = nullptr);
    ~SystemTray() override;

    void show();
    void retranslateUi();

    // Set sprite pack manager for pack submenu
    void setSpritePackManager(SpritePackManager *manager);

    // Set update checker
    void setUpdateChecker(UpdateChecker *checker);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onPackActionTriggered();
    void onUpdateAvailable(const QString &current, const QString &latest, const QString &url);
    void onNoUpdateAvailable(const QString &current);
    void onUpdateCheckFailed(const QString &error);

private:
    void setupMenu();
    void refreshPackMenu();

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QMenu *m_packMenu = nullptr;
    QWidget *m_mainWindow = nullptr;
    QAction *m_toggleAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_checkUpdateAction = nullptr;
    SpritePackManager *m_packManager = nullptr;
    UpdateChecker *m_updateChecker = nullptr;
};

#endif // SYSTEMTRAY_H

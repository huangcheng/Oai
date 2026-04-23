#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QObject>
#include <QSystemTrayIcon>

class QSystemTrayIcon;
class QMenu;
class QWidget;
class QAction;
class SpritePackManager;

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

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onPackActionTriggered();

private:
    void setupMenu();
    void refreshPackMenu();

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QMenu *m_packMenu = nullptr;
    QWidget *m_mainWindow = nullptr;
    QAction *m_toggleAction = nullptr;
    QAction *m_quitAction = nullptr;
    SpritePackManager *m_packManager = nullptr;
};

#endif // SYSTEMTRAY_H

#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QObject>
#include <QSystemTrayIcon>

class QSystemTrayIcon;
class QMenu;
class QWidget;
class QAction;

class SystemTray : public QObject
{
    Q_OBJECT

public:
    explicit SystemTray(QWidget *mainWindow, QObject *parent = nullptr);
    ~SystemTray() override;

    void show();
    void retranslateUi();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupMenu();

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QWidget *m_mainWindow = nullptr;
    QAction *m_toggleAction = nullptr;
    QAction *m_quitAction = nullptr;
};

#endif // SYSTEMTRAY_H

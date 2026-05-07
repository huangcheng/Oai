#ifndef GLOBALSHORTCUTMANAGER_H
#define GLOBALSHORTCUTMANAGER_H

#include <QObject>
#include <QString>

class QHotkey;

class GlobalShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(QObject *parent = nullptr);
    ~GlobalShortcutManager() override;

    void setShortcut(const QString &shortcut);
    QString shortcut() const;

    void setEnabled(bool enabled);
    bool enabled() const;

signals:
    void activated();
    void shortcutChanged(const QString &newShortcut);

private:
    QHotkey *m_hotkey = nullptr;
    QString m_shortcut;
    bool m_enabled = true;
};

#endif // GLOBALSHORTCUTMANAGER_H

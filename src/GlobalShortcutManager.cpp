#include "GlobalShortcutManager.h"
#include <QHotkey>
#include <QDebug>

GlobalShortcutManager::GlobalShortcutManager(QObject *parent)
    : QObject(parent)
    , m_shortcut(QStringLiteral("Ctrl+Shift+O"))
{
    m_hotkey = new QHotkey(QKeySequence(m_shortcut), true, this);
    connect(m_hotkey, &QHotkey::activated, this, &GlobalShortcutManager::activated);
}

GlobalShortcutManager::~GlobalShortcutManager() = default;

void GlobalShortcutManager::setShortcut(const QString &shortcut)
{
    if (m_shortcut == shortcut) return;
    m_shortcut = shortcut;
    if (m_hotkey) {
        m_hotkey->setShortcut(QKeySequence(shortcut), false);
        if (m_enabled) {
            m_hotkey->setRegistered(true);
        }
    }
    emit shortcutChanged(shortcut);
}

QString GlobalShortcutManager::shortcut() const { return m_shortcut; }

void GlobalShortcutManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    if (m_hotkey) {
        m_hotkey->setRegistered(enabled);
    }
}

bool GlobalShortcutManager::enabled() const { return m_enabled; }

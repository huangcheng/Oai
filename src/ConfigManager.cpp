#include "ConfigManager.h"

#include <QDebug>

QString ConfigManager::defaultEndpoint()
{
    return QStringLiteral("127.0.0.1:52847");
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, "Oai", "Oai")
    , m_ipcEndpoint(defaultEndpoint())
{
}

void ConfigManager::load()
{
    if (!m_settings.contains("language")) {
        qDebug() << "No config file found, using defaults";
        return;
    }

    // Window position
    m_windowPosition.setX(m_settings.value("windowX", 0).toInt());
    m_windowPosition.setY(m_settings.value("windowY", 0).toInt());

    // Language
    m_language = m_settings.value("language", "en").toString();

    // Auto-start
    m_autoStart = m_settings.value("autoStart", false).toBool();

    // IPC endpoint
    QString endpoint = m_settings.value("ipcEndpoint").toString();
    if (!endpoint.isEmpty()) {
        m_ipcEndpoint = endpoint;
    }

    // Last-selected character pack
    m_activePackId = m_settings.value("activePackId").toString();

    qDebug() << "Config loaded from:" << m_settings.fileName();
}

void ConfigManager::save()
{
    m_settings.setValue("windowX", m_windowPosition.x());
    m_settings.setValue("windowY", m_windowPosition.y());
    m_settings.setValue("language", m_language);
    m_settings.setValue("autoStart", m_autoStart);
    m_settings.setValue("ipcEndpoint", m_ipcEndpoint);
    m_settings.setValue("activePackId", m_activePackId);
    m_settings.sync();
}

void ConfigManager::setWindowPosition(const QPoint &pos)
{
    if (m_windowPosition != pos) {
        m_windowPosition = pos;
        save();
    }
}

void ConfigManager::setLanguage(const QString &lang)
{
    if (m_language != lang) {
        m_language = lang;
        save();
        emit languageChanged(lang);
    }
}

void ConfigManager::setAutoStart(bool enabled)
{
    if (m_autoStart != enabled) {
        m_autoStart = enabled;
        save();
    }
}

void ConfigManager::setActivePackId(const QString &packId)
{
    if (m_activePackId != packId) {
        m_activePackId = packId;
        save();
    }
}

quint16 ConfigManager::ipcPort() const
{
    int colonPos = m_ipcEndpoint.lastIndexOf(':');
    if (colonPos > 0) {
        return static_cast<quint16>(m_ipcEndpoint.mid(colonPos + 1).toUInt());
    }
    return 52847;
}

void ConfigManager::setIpcPort(quint16 port)
{
    const QString newEndpoint = QStringLiteral("127.0.0.1:") + QString::number(port);
    if (m_ipcEndpoint != newEndpoint) {
        m_ipcEndpoint = newEndpoint;
        save();
        emit ipcEndpointChanged(m_ipcEndpoint);
    }
}

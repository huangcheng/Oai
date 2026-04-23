#include "ConfigManager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

QString ConfigManager::defaultEndpoint()
{
    // TCP localhost — same on all platforms. Port 52847 is chosen from the
    // IANA dynamic/private range (49152-65535) to avoid conflicts with
    // registered services.
    return QStringLiteral("127.0.0.1:52847");
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_ipcEndpoint(defaultEndpoint())
{
}

void ConfigManager::load()
{
    const QString path = configFilePath();
    QFile file(path);

    if (!file.exists()) {
        qDebug() << "No config file found, using defaults";
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << path;
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Config parse error:" << error.errorString();
        return;
    }

    QJsonObject obj = doc.object();

    // Window position
    if (obj.contains("windowX") && obj.contains("windowY")) {
        m_windowPosition = QPoint(obj["windowX"].toInt(), obj["windowY"].toInt());
    }

    // Language
    if (obj.contains("language")) {
        m_language = obj["language"].toString();
    }

    // Auto-start
    if (obj.contains("autoStart")) {
        m_autoStart = obj["autoStart"].toBool();
    }

    // IPC endpoint override
    if (obj.contains("ipcEndpoint")) {
        m_ipcEndpoint = obj["ipcEndpoint"].toString();
    } else if (obj.contains("ipcSocketPath")) {
        // Backward compat with old config field name
        m_ipcEndpoint = obj["ipcSocketPath"].toString();
    }

    qDebug() << "Config loaded from:" << path;
}

void ConfigManager::save()
{
    const QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject obj;
    obj["windowX"] = m_windowPosition.x();
    obj["windowY"] = m_windowPosition.y();
    obj["language"] = m_language;
    obj["autoStart"] = m_autoStart;
    obj["ipcEndpoint"] = m_ipcEndpoint;

    QJsonDocument doc(obj);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to write config file:" << path;
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Config saved to:" << path;
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

QString ConfigManager::configFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + "/Orai/config.json";
}

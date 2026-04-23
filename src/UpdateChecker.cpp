#include "UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDebug>

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

QString UpdateChecker::currentVersion()
{
    // Version from CMakeLists.txt project() command
    return QStringLiteral(PROJECT_VERSION);
}

void UpdateChecker::checkForUpdates()
{
    qDebug() << "UpdateChecker: Checking for updates...";

    QUrl url(GITHUB_API_URL);
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "Orai-Desktop-Pet");

    m_networkManager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QString error = reply->errorString();
        qWarning() << "UpdateChecker: Network error:" << error;
        emit checkFailed(error);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QString error = parseError.errorString();
        qWarning() << "UpdateChecker: JSON parse error:" << error;
        emit checkFailed(error);
        return;
    }

    QJsonObject release = doc.object();
    QString tagName = release.value("tag_name").toString();
    QString downloadUrl;

    // Find the download URL for the current platform
    QJsonArray assets = release.value("assets").toArray();
    for (const QJsonValue &asset : assets) {
        QJsonObject assetObj = asset.toObject();
        QString name = assetObj.value("name").toString().toLower();

#ifdef Q_OS_WIN
        if (name.contains("windows") || name.contains("win") || name.endsWith(".exe")) {
            downloadUrl = assetObj.value("browser_download_url").toString();
            break;
        }
#elif defined(Q_OS_MAC)
        if (name.contains("macos") || name.contains("darwin") || name.endsWith(".dmg")) {
            downloadUrl = assetObj.value("browser_download_url").toString();
            break;
        }
#elif defined(Q_OS_LINUX)
        if (name.contains("linux") || name.endsWith(".tar.gz")) {
            downloadUrl = assetObj.value("browser_download_url").toString();
            break;
        }
#endif
    }

    // If no platform-specific asset found, use the first asset
    if (downloadUrl.isEmpty() && !assets.isEmpty()) {
        downloadUrl = assets[0].toObject().value("browser_download_url").toString();
    }

    // Compare versions
    QString currentVer = currentVersion();
    QString latestVer = tagName.startsWith("v") ? tagName.mid(1) : tagName;

    qDebug() << "UpdateChecker: Current:" << currentVer << "Latest:" << latestVer;

    if (latestVer != currentVer) {
        qDebug() << "UpdateChecker: Update available!";
        emit updateAvailable(currentVer, latestVer, downloadUrl);
    } else {
        qDebug() << "UpdateChecker: Already up to date.";
        emit noUpdateAvailable(currentVer);
    }
}

#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    // Check for updates from GitHub releases
    void checkForUpdates();

    // Get current version
    static QString currentVersion();

signals:
    // Emitted when an update is available
    void updateAvailable(const QString &currentVersion, const QString &latestVersion, const QString &downloadUrl);

    // Emitted when no update is available
    void noUpdateAvailable(const QString &currentVersion);

    // Emitted when check fails
    void checkFailed(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    static constexpr const char *GITHUB_API_URL = "https://api.github.com/repos/huangcheng/Oai/releases/latest";
};

#endif // UPDATECHECKER_H

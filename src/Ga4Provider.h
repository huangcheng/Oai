#ifndef GA4PROVIDER_H
#define GA4PROVIDER_H

#include "AnalyticsProvider.h"
#include <QObject>
#include <QString>

class QNetworkAccessManager;

/**
 * @brief Google Analytics 4 provider via Measurement Protocol
 *
 * Fires-and-forgets HTTP POSTs to GA4. Network failures are silently
 * ignored — analytics is best-effort telemetry, never a hard dependency.
 *
 * Compile-time credentials via CMake:
 *   - OAI_ANALYTICS_MEASUREMENT_ID
 *   - OAI_ANALYTICS_API_SECRET
 */
class Ga4Provider : public QObject, public AnalyticsProvider
{
    Q_OBJECT

public:
    explicit Ga4Provider(QObject* parent = nullptr);
    ~Ga4Provider() override = default;

    void trackEvent(const QJsonObject& event) override;
    QString providerName() const override { return QStringLiteral("ga4"); }

private:
    QJsonObject buildPayload(const QJsonObject& event) const;
    QString clientId() const;
    QString makeEventName(const QString& ipcEventName) const;

    QNetworkAccessManager* m_network = nullptr;
    QString m_clientId;
    QString m_sessionId;
    qint64 m_sessionStartMs = 0;
    QString m_endpoint;
};

#endif // GA4PROVIDER_H
#include "Ga4Provider.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QSettings>
#include <QSysInfo>
#include <QCoreApplication>
#include <QDateTime>

// GA4 Measurement Protocol endpoint
static const char kGa4Endpoint[] = "https://www.google-analytics.com/mp/collect";

// Helper macros for preprocessor stringification
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Compile-time credentials — passed from CMake via -D
static const char kMeasurementId[] = TOSTRING(OAI_ANALYTICS_MEASUREMENT_ID);
static const char kApiSecret[] = TOSTRING(OAI_ANALYTICS_API_SECRET);

// Client ID storage key
static const char kClientIdKey[] = "analyticsClientId";

Ga4Provider::Ga4Provider(QObject* parent)
    : QObject(parent)
{
    // Build endpoint URL
    m_endpoint = QStringLiteral("%1?measurement_id=%2&api_secret=%3")
                     .arg(kGa4Endpoint, kMeasurementId, kApiSecret);

    // Get or create persistent client ID
    QSettings settings;
    m_clientId = settings.value(kClientIdKey).toString();
    if (m_clientId.isEmpty()) {
        m_clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(kClientIdKey, m_clientId);
    }

    // Create network manager
    m_network = new QNetworkAccessManager(this);

    // Generate a new session ID for this app launch
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sessionStartMs = QDateTime::currentMSecsSinceEpoch();
}

void Ga4Provider::trackEvent(const QJsonObject& event)
{
    if (!m_network) {
        return;
    }

    // Build GA4 payload
    QJsonObject payload = buildPayload(event);

    // Fire and forget — no callbacks, no error handling, no retry
    QNetworkRequest request{QUrl(m_endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // QNetworkAccessManager::post() is async — returns immediately
    // The reply is intentionally NOT stored; Qt deletes it when done
    // If network fails, the event is silently dropped — Oai continues normally
    QNetworkReply *reply = m_network->post(request, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Ga4Provider: error:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

QJsonObject Ga4Provider::buildPayload(const QJsonObject& event) const
{
    const QString eventName = makeEventName(event.value("event").toString());
    const QString source = event.value("source").toString();

    QJsonObject params;
    // App info
    params["app_version"] = QString::fromLatin1(STRINGIFY(PROJECT_VERSION));
    params["app_id"] = QStringLiteral("com.oai.desktop");
    // System info
    params["os_type"] = QSysInfo::productType();          // windows, macos, linux
    params["qt_version"] = qVersion();                    // Qt runtime version
    // Session & engagement — required for GA4 to count users/sessions
    params["session_id"] = m_sessionId;
    params["engagement_time_msec"] = static_cast<qint64>(
        QDateTime::currentMSecsSinceEpoch() - m_sessionStartMs);

    params["event_source"] = source;
    if (event.contains("toolName")) {
        params["tool_name"] = event.value("toolName").toString();
    }
    if (event.contains("filePath")) {
        params["file_path"] = event.value("filePath").toString();
    }

    QJsonArray events;
    QJsonObject gaEvent;
    gaEvent["name"] = eventName;
    gaEvent["params"] = params;
    events.append(gaEvent);

    QJsonObject payload;
    payload["client_id"] = m_clientId;
    payload["events"] = events;

    return payload;
}

QString Ga4Provider::makeEventName(const QString& ipcEventName) const
{
    // GA4 event names: lowercase, underscores only, no spaces, max 40 chars
    QString name = ipcEventName;
    name = name.replace('.', '_');
    name = name.replace('-', '_');
    name = name.toLower();
    if (name.length() > 40) {
        name = name.left(40);
    }
    return name;
}
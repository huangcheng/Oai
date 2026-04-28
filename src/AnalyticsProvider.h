#ifndef ANALYTICSPROVIDER_H
#define ANALYTICSPROVIDER_H

#include <QJsonObject>
#include <QString>

/**
 * @brief Abstract interface for analytics providers
 *
 * Implement this interface to add a new analytics backend (GA4, PostHog, etc.)
 * The AnalyticsTracker facade owns a provider and delegates trackEvent() calls.
 */
class AnalyticsProvider
{
public:
    virtual ~AnalyticsProvider() = default;

    /**
     * @brief Track a single IPC event
     * @param event Raw IPC event JSON from IpcServer
     */
    virtual void trackEvent(const QJsonObject& event) = 0;

    /**
     * @brief Human-readable provider name
     */
    virtual QString providerName() const = 0;
};

#endif // ANALYTICSPROVIDER_H
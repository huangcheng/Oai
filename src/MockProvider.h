#ifndef MOCKPROVIDER_H
#define MOCKPROVIDER_H

#include "AnalyticsProvider.h"

/**
 * @brief Mock analytics provider — no-op implementation
 *
 * Used when analytics is compiled in but disabled, or for testing.
 * All trackEvent() calls are silently dropped.
 */
class MockProvider : public AnalyticsProvider
{
public:
    void trackEvent(const QJsonObject& event) override;
    QString providerName() const override { return QStringLiteral("mock"); }
};

#endif // MOCKPROVIDER_H
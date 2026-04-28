#include "AnalyticsTracker.h"
#include "AnalyticsProvider.h"

#ifdef OAI_ANALYTICS_ENABLED
#ifdef OAI_ANALYTICS_PROVIDER_GA4
#include "Ga4Provider.h"
#elif defined(OAI_ANALYTICS_PROVIDER_MOCK)
#include "MockProvider.h"
#endif
#endif

AnalyticsTracker::AnalyticsTracker(bool enabled, QObject* parent)
    : QObject(parent)
    , m_enabled(enabled)
{
#ifdef OAI_ANALYTICS_ENABLED
#ifdef OAI_ANALYTICS_PROVIDER_GA4
    m_provider = std::make_unique<Ga4Provider>(this);
#elif defined(OAI_ANALYTICS_PROVIDER_MOCK)
    m_provider = std::make_unique<MockProvider>();
#endif
#endif
}

AnalyticsTracker::~AnalyticsTracker() = default;

void AnalyticsTracker::trackEvent(const QJsonObject& event)
{
    if (!m_enabled || !m_provider) {
        return;
    }
    m_provider->trackEvent(event);
}
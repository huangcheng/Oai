#include "MockProvider.h"

void MockProvider::trackEvent(const QJsonObject& event)
{
    Q_UNUSED(event);
    // No-op: analytics disabled or testing mode
}
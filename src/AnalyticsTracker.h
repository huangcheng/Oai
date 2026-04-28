#ifndef ANALYTICSTRACKER_H
#define ANALYTICSTRACKER_H

#include <QObject>
#include <QJsonObject>
#include <memory>

class AnalyticsProvider;

/**
 * @brief Analytics tracker facade — owns and delegates to an AnalyticsProvider
 *
 * Connects to IpcServer::eventReceived and forwards events to the configured
 * analytics provider. The provider is selected at compile-time via CMake
 * (ga4 or mock).
 *
 * When OAI_ANALYTICS_ENABLED is not defined, this class is a no-op stub.
 */
class AnalyticsTracker : public QObject
{
    Q_OBJECT

public:
    explicit AnalyticsTracker(bool enabled, QObject* parent = nullptr);
    ~AnalyticsTracker() override;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

public slots:
    void trackEvent(const QJsonObject& event);

private:
    bool m_enabled = false;
    std::unique_ptr<AnalyticsProvider> m_provider;
};

#endif // ANALYTICSTRACKER_H
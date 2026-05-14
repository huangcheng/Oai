/**
 * test_tts_live.cpp — Manual live-API smoke test.
 *
 * Skipped unless OAI_LIVE_TTS=1 is set in the environment. Reads
 * credentials from per-provider env vars and exercises each adapter
 * against the real API. Run before releases.
 *
 *   OAI_LIVE_TTS=1 \
 *     OAI_STEPFUN_TOKEN=... \
 *     OAI_MINIMAX_TOKEN=... OAI_MINIMAX_GROUP=... \
 *     OAI_AZURE_KEY=...    OAI_AZURE_REGION=eastus \
 *     OAI_OPENAI_TOKEN=... \
 *     ./test_tts_live
 */

#include <QtTest>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <cstdlib>

#include "tts/StepFunHttpProvider.h"
#include "tts/MiniMaxHttpProvider.h"
#include "tts/AzureSpeechProvider.h"
#include "tts/OpenAiTtsProvider.h"

using namespace oai::tts;

namespace {
QString env(const char* name) { return QString::fromLocal8Bit(qgetenv(name)); }
} // namespace

class TestTtsLive : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        if (env("OAI_LIVE_TTS") != "1")
            QSKIP("Set OAI_LIVE_TTS=1 to run live-API tests");
        m_nam = new QNetworkAccessManager(this);
    }

    void stepFun() {
        const QString token = env("OAI_STEPFUN_TOKEN");
        if (token.isEmpty()) QSKIP("OAI_STEPFUN_TOKEN not set");
        ProviderConfig cfg{{
            {"baseUrl", "https://api.stepfun.com"},
            {"token", token},
            {"voice", "cixingnansheng"},
        }};
        StepFunHttpProvider provider(cfg, m_nam);
        runOne(provider);
    }

    void miniMax() {
        const QString token = env("OAI_MINIMAX_TOKEN");
        const QString group = env("OAI_MINIMAX_GROUP");
        if (token.isEmpty() || group.isEmpty())
            QSKIP("OAI_MINIMAX_TOKEN and OAI_MINIMAX_GROUP required");
        ProviderConfig cfg{{
            {"baseUrl", "https://api.minimaxi.com"},
            {"token", token}, {"groupId", group},
            {"voice", "female-shaonv"},
        }};
        MiniMaxHttpProvider provider(cfg, m_nam);
        runOne(provider);
    }

    void azure() {
        const QString key = env("OAI_AZURE_KEY");
        const QString region = env("OAI_AZURE_REGION");
        if (key.isEmpty() || region.isEmpty())
            QSKIP("OAI_AZURE_KEY and OAI_AZURE_REGION required");
        ProviderConfig cfg{{
            {"key", key}, {"region", region},
            {"voice", "en-US-JennyNeural"},
        }};
        AzureSpeechProvider provider(cfg, m_nam);
        runOne(provider);
    }

    void openAi() {
        const QString token = env("OAI_OPENAI_TOKEN");
        if (token.isEmpty()) QSKIP("OAI_OPENAI_TOKEN not set");
        ProviderConfig cfg{{
            {"baseUrl", "https://api.openai.com/v1"},
            {"token", token}, {"voice", "nova"},
        }};
        OpenAiTtsProvider provider(cfg, m_nam);
        runOne(provider);
    }

private:
    void runOne(ITtsProvider& provider) {
        QByteArray audio;
        QString err;
        QEventLoop loop;
        provider.synthesize({QStringLiteral("Oai live test."), {}},
            [&](SynthesisResult r){ audio = r.audio; loop.quit(); },
            [&](TtsError e){ err = e.message; loop.quit(); });
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(audio.size() > 1024);
    }

    QNetworkAccessManager* m_nam = nullptr;
};

QTEST_MAIN(TestTtsLive)
#include "test_tts_live.moc"

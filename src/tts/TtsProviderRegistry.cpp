#include "TtsProviderRegistry.h"

#include <QCoreApplication>
#include <stdexcept>

namespace oai::tts {

namespace {

const QList<ProviderDescriptor>& builtInDescriptors()
{
    static const QList<ProviderDescriptor> kDescriptors = {
        ProviderDescriptor{
            TtsProviderId::StepFun,
            QStringLiteral("stepfun"),
            QStringLiteral("StepFun"),
            QStringList{QStringLiteral("token"), QStringLiteral("voice")},
            QStringList{QStringLiteral("baseUrl"), QStringLiteral("model")},
            {
                {QStringLiteral("cixingnansheng"),
                 QCoreApplication::translate("Tts", "Cixingnansheng (male)"),
                 QStringLiteral("zh-CN")},
                {QStringLiteral("linjiajiejie"),
                 QCoreApplication::translate("Tts", "Linjiajiejie (female)"),
                 QStringLiteral("zh-CN")},
            },
            {Emotion::Neutral, Emotion::Happy, Emotion::Sad,
             Emotion::Angry, Emotion::Calm, Emotion::Whisper},
            // Factory wired in Task 5.
            [](const ProviderConfig&, QNetworkAccessManager*)
                -> std::unique_ptr<ITtsProvider> {
                throw std::runtime_error("StepFun provider not yet implemented");
            },
        },
        ProviderDescriptor{
            TtsProviderId::MiniMax,
            QStringLiteral("minimax"),
            QStringLiteral("MiniMax"),
            QStringList{QStringLiteral("token"),
                        QStringLiteral("groupId"),
                        QStringLiteral("voice")},
            QStringList{QStringLiteral("model")},
            {
                {QStringLiteral("female-shaonv"),
                 QCoreApplication::translate("Tts", "Female young (shaonv)"),
                 QStringLiteral("zh-CN")},
                {QStringLiteral("male-qn-qingse"),
                 QCoreApplication::translate("Tts", "Male qingse"),
                 QStringLiteral("zh-CN")},
            },
            {Emotion::Neutral, Emotion::Happy, Emotion::Sad,
             Emotion::Angry, Emotion::Calm, Emotion::Whisper},
            [](const ProviderConfig&, QNetworkAccessManager*)
                -> std::unique_ptr<ITtsProvider> {
                throw std::runtime_error("MiniMax provider not yet implemented");
            },
        },
        ProviderDescriptor{
            TtsProviderId::Azure,
            QStringLiteral("azure"),
            QStringLiteral("Azure Speech"),
            QStringList{QStringLiteral("key"),
                        QStringLiteral("region"),
                        QStringLiteral("voice")},
            {},
            {
                {QStringLiteral("zh-CN-XiaoxiaoNeural"),
                 QCoreApplication::translate("Tts", "Xiaoxiao (zh-CN, female)"),
                 QStringLiteral("zh-CN")},
                {QStringLiteral("en-US-JennyNeural"),
                 QCoreApplication::translate("Tts", "Jenny (en-US, female)"),
                 QStringLiteral("en-US")},
            },
            {Emotion::Neutral, Emotion::Happy, Emotion::Sad,
             Emotion::Angry, Emotion::Calm, Emotion::Whisper},
            [](const ProviderConfig&, QNetworkAccessManager*)
                -> std::unique_ptr<ITtsProvider> {
                throw std::runtime_error("Azure provider not yet implemented");
            },
        },
        ProviderDescriptor{
            TtsProviderId::OpenAI,
            QStringLiteral("openai"),
            QStringLiteral("OpenAI"),
            QStringList{QStringLiteral("token"), QStringLiteral("voice")},
            QStringList{QStringLiteral("baseUrl"), QStringLiteral("model")},
            {
                {QStringLiteral("alloy"),    QStringLiteral("Alloy"),   QStringLiteral("en")},
                {QStringLiteral("nova"),     QStringLiteral("Nova"),    QStringLiteral("en")},
                {QStringLiteral("shimmer"),  QStringLiteral("Shimmer"), QStringLiteral("en")},
                {QStringLiteral("echo"),     QStringLiteral("Echo"),    QStringLiteral("en")},
            },
            {Emotion::Neutral},
            [](const ProviderConfig&, QNetworkAccessManager*)
                -> std::unique_ptr<ITtsProvider> {
                throw std::runtime_error("OpenAI provider not yet implemented");
            },
        },
    };
    return kDescriptors;
}

} // namespace

const QList<ProviderDescriptor>& TtsProviderRegistry::descriptors()
{
    return builtInDescriptors();
}

const ProviderDescriptor* TtsProviderRegistry::find(TtsProviderId id)
{
    for (const auto& d : descriptors())
        if (d.id == id) return &d;
    return nullptr;
}

const ProviderDescriptor* TtsProviderRegistry::findByStableId(const QString& stableId)
{
    for (const auto& d : descriptors())
        if (d.stableId == stableId) return &d;
    return nullptr;
}

} // namespace oai::tts

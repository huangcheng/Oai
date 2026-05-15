#include <QtTest>
#include <QFile>

#include "tts/TtsVoiceCache.h"

using namespace seelie::tts;

class TestTtsVoiceCache : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void testCacheKeyDeterministic();
    void testCacheKeyDifferentPerProvider();
    void testCacheKeyDifferentPerVoice();
    void testCacheKeyDifferentPerModel();
    void testCacheKeyDifferentPerOptions();
    void testCacheKeyNormalizesWhitespace();
    void testCacheHit();
    void testCacheMiss();
    void testWipeAll();
    void testEmptyAudioNotCached();
    void testMimeTypeRoundTrip();
    void testLruEvictionOnOverflow();
};

void TestTtsVoiceCache::initTestCase()
{
    // Use an isolated cache dir per test run to keep results deterministic
    // and avoid clobbering any real cache on the developer's machine.
    qputenv("XDG_CACHE_HOME", "/tmp/seelie_test_cache");
    QStandardPaths::setTestModeEnabled(true);
}

void TestTtsVoiceCache::cleanup()
{
    TtsVoiceCache cache;
    cache.wipeAll();
}

void TestTtsVoiceCache::testCacheKeyDeterministic()
{
    SpeakOptions opts;
    const QString k1 = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"),
        QStringLiteral("cixingnansheng"),
        QStringLiteral("stepaudio-2.5-tts"),
        opts,
        QStringLiteral("Hello"));
    const QString k2 = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"),
        QStringLiteral("cixingnansheng"),
        QStringLiteral("stepaudio-2.5-tts"),
        opts,
        QStringLiteral("Hello"));
    QCOMPARE(k1, k2);
    QCOMPARE(k1.length(), 64);
}

void TestTtsVoiceCache::testCacheKeyDifferentPerProvider()
{
    SpeakOptions opts;
    const QString k1 = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("voice1"), QStringLiteral("model1"), opts, QStringLiteral("Hello"));
    const QString k2 = TtsVoiceCache::cacheKey(
        QStringLiteral("minimax"), QStringLiteral("voice1"), QStringLiteral("model1"), opts, QStringLiteral("Hello"));
    QVERIFY(k1 != k2);
}

void TestTtsVoiceCache::testCacheKeyDifferentPerVoice()
{
    SpeakOptions opts;
    const QString k1 = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("voice1"), QStringLiteral("model1"), opts, QStringLiteral("Hello"));
    const QString k2 = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("voice2"), QStringLiteral("model1"), opts, QStringLiteral("Hello"));
    QVERIFY(k1 != k2);
}

void TestTtsVoiceCache::testCacheKeyDifferentPerModel()
{
    SpeakOptions opts;
    const QString k1 = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("voice1"), QStringLiteral("model1"), opts, QStringLiteral("Hello"));
    const QString k2 = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("voice1"), QStringLiteral("model2"), opts, QStringLiteral("Hello"));
    QVERIFY(k1 != k2);
}

void TestTtsVoiceCache::testCacheKeyDifferentPerOptions()
{
    SpeakOptions a;
    SpeakOptions b;
    b.rate = 1.5f;
    SpeakOptions c;
    c.emotion = Emotion::Happy;

    const QString ka = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("v"), QStringLiteral("m"), a, QStringLiteral("Hi"));
    const QString kb = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("v"), QStringLiteral("m"), b, QStringLiteral("Hi"));
    const QString kc = TtsVoiceCache::cacheKey(
        QStringLiteral("stepfun"), QStringLiteral("v"), QStringLiteral("m"), c, QStringLiteral("Hi"));
    QVERIFY(ka != kb);
    QVERIFY(ka != kc);
    QVERIFY(kb != kc);
}

void TestTtsVoiceCache::testCacheKeyNormalizesWhitespace()
{
    SpeakOptions opts;
    const QString k1 = TtsVoiceCache::cacheKey(
        QStringLiteral("p"), QStringLiteral("v"), QStringLiteral("m"), opts, QStringLiteral("Hello"));
    const QString k2 = TtsVoiceCache::cacheKey(
        QStringLiteral("p"), QStringLiteral("v"), QStringLiteral("m"), opts, QStringLiteral(" Hello "));
    const QString k3 = TtsVoiceCache::cacheKey(
        QStringLiteral("p"), QStringLiteral("v"), QStringLiteral("m"), opts, QStringLiteral("Hello\n"));
    QCOMPARE(k1, k2);
    QCOMPARE(k1, k3);
}

void TestTtsVoiceCache::testCacheHit()
{
    TtsVoiceCache cache;
    const QString key = QStringLiteral("testkey123");
    const QByteArray audio = QByteArray("fake mp3 data");

    QVERIFY(!cache.hasCachedAudio(key));
    QVERIFY(cache.writeCachedAudio(key, audio, QStringLiteral("audio/mpeg")));
    QVERIFY(cache.hasCachedAudio(key));
    QCOMPARE(cache.getCachedAudio(key), audio);
}

void TestTtsVoiceCache::testCacheMiss()
{
    TtsVoiceCache cache;
    const QString key = QStringLiteral("nonexistent_key_12345");
    QVERIFY(!cache.hasCachedAudio(key));
    QVERIFY(cache.getCachedAudio(key).isEmpty());
}

void TestTtsVoiceCache::testWipeAll()
{
    TtsVoiceCache cache;
    const QString key1 = QStringLiteral("key1");
    const QString key2 = QStringLiteral("key2");

    cache.writeCachedAudio(key1, QByteArray("audio1"), QStringLiteral("audio/mpeg"));
    cache.writeCachedAudio(key2, QByteArray("audio2"), QStringLiteral("audio/mpeg"));
    QVERIFY(cache.hasCachedAudio(key1));
    QVERIFY(cache.hasCachedAudio(key2));

    cache.wipeAll();
    QVERIFY(!cache.hasCachedAudio(key1));
    QVERIFY(!cache.hasCachedAudio(key2));
}

void TestTtsVoiceCache::testEmptyAudioNotCached()
{
    TtsVoiceCache cache;
    const QString key = QStringLiteral("empty_key");
    QVERIFY(!cache.writeCachedAudio(key, QByteArray(), QStringLiteral("audio/mpeg")));
    QVERIFY(!cache.hasCachedAudio(key));
    // No sidecar should have been created either.
    QVERIFY(cache.getCachedMimeType(key).isEmpty());
}

void TestTtsVoiceCache::testMimeTypeRoundTrip()
{
    TtsVoiceCache cache;
    const QString key = QStringLiteral("mime_roundtrip");
    const QByteArray audio = QByteArray("RIFF....WAVEfmt ", 16);
    QVERIFY(cache.writeCachedAudio(key, audio, QStringLiteral("audio/wav")));
    QCOMPARE(cache.getCachedMimeType(key), QStringLiteral("audio/wav"));
}

void TestTtsVoiceCache::testLruEvictionOnOverflow()
{
    TtsVoiceCache cache;
    // Cap fits two ~1 KB audio entries (plus their tiny mime sidecars) but
    // not a third. After three writes the oldest must be evicted.
    cache.setMaxBytes(2500);

    const QByteArray payload(1024, 'x');
    QVERIFY(cache.writeCachedAudio(QStringLiteral("oldest"), payload, QStringLiteral("audio/mpeg")));
    // Bump mtime ordering deterministically (HFS+/APFS mtime granularity is
    // 1s, so wait slightly more than that between writes).
    QTest::qWait(1100);
    QVERIFY(cache.writeCachedAudio(QStringLiteral("middle"), payload, QStringLiteral("audio/mpeg")));
    QTest::qWait(1100);
    QVERIFY(cache.writeCachedAudio(QStringLiteral("newest"), payload, QStringLiteral("audio/mpeg")));

    QVERIFY(!cache.hasCachedAudio(QStringLiteral("oldest")));
    QVERIFY(cache.hasCachedAudio(QStringLiteral("middle")));
    QVERIFY(cache.hasCachedAudio(QStringLiteral("newest")));
}

QTEST_MAIN(TestTtsVoiceCache)
#include "test_tts_voice_cache.moc"

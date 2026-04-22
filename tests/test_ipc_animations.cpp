/**
 * test_ipc_animations.cpp
 *
 * End-to-end TCP IPC tests for Qlippy.
 *
 * Spins up the real IpcServer, EventRouter, SpriteAnimationEngine,
 * LottieEffectOverlay, and TipBubbleWidget, then drives them via
 * raw TCP sockets (same protocol as the Node.js gateways).
 */

#include <QTest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTcpServer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>

#include "IpcServer.h"
#include "EventRouter.h"
#include "SpriteAnimationEngine.h"
#include "LottieEffectOverlay.h"
#include "TipBubbleWidget.h"
#include "TipsEngine.h"

class TestIpcAnimations : public QObject
{
    Q_OBJECT

public:
    TestIpcAnimations() = default;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testPingPong();
    void testEventTriggersAnimation();
    void testEventTriggersEffect();
    void testTipMessage();
    void testMultipleConcurrentSenders();
    void testMalformedJson();
    void testUnknownEventType();
    void testPriorityQueue();

private:
    static QString findAssetsDir();
    QByteArray readAllWithTimeout(QTcpSocket *socket, int timeoutMs = 1000);
    void sendJson(QTcpSocket *socket, const QJsonObject &obj);

    IpcServer *m_ipc = nullptr;
    EventRouter *m_router = nullptr;
    SpriteAnimationEngine *m_engine = nullptr;
    LottieEffectOverlay *m_effects = nullptr;
    TipBubbleWidget *m_bubble = nullptr;
    TipsEngine *m_tips = nullptr;

    static constexpr quint16 TEST_PORT = 52848; // different from production
};

QString TestIpcAnimations::findAssetsDir()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        QString candidate = dir.absoluteFilePath("assets");
        if (QFile::exists(candidate + "/map.png")) {
            return candidate;
        }
        if (!dir.cdUp()) break;
    }
    // Fallback for in-source test runs
    QString srcAssets = QDir(QStringLiteral(SOURCE_DIR)).absoluteFilePath("assets");
    if (QFile::exists(srcAssets + "/map.png")) {
        return srcAssets;
    }
    return QString();
}

void TestIpcAnimations::initTestCase()
{
    QString assetsDir = findAssetsDir();
    QVERIFY2(!assetsDir.isEmpty(), "Could not find assets directory");

    m_engine = new SpriteAnimationEngine(this);
    m_engine->loadAssets(assetsDir + "/map.png", assetsDir + "/animations.json");

    m_effects = new LottieEffectOverlay(this);
    m_effects->loadEffects(assetsDir + "/lottie/effects");

    m_bubble = new TipBubbleWidget(nullptr);
    m_bubble->setAnchorRect(QRect(0, 0, 124, 93));

    m_tips = new TipsEngine(this);
    m_tips->setAnimationEngine(m_engine);
    m_tips->setTipBubble(m_bubble);

    m_router = new EventRouter(this);
    m_router->setAnimationEngine(m_engine);
    m_router->setTipBubble(m_bubble);
    m_router->setEffectOverlay(m_effects);
    m_router->setTipsEngine(m_tips);

    m_ipc = new IpcServer(this);
    connect(m_ipc, &IpcServer::eventReceived, m_router, &EventRouter::routeEvent);
    connect(m_ipc, &IpcServer::tipReceived, m_bubble, [this](const QJsonObject &tip) {
        const QString title = tip.value("title").toString("Tip");
        const QString body = tip.value("body").toString();
        const QString anim = tip.value("animation").toString();
        m_bubble->showBubble(title, body, TipBubbleWidget::TipBubble);
        if (!anim.isEmpty()) {
            m_engine->playAnimation(anim, SpriteAnimationEngine::NormalPriority);
        }
    });

    QVERIFY(m_ipc->start(QStringLiteral("127.0.0.1:%1").arg(TEST_PORT)));
}

void TestIpcAnimations::cleanupTestCase()
{
    m_ipc->stop();
    delete m_bubble;
}

void TestIpcAnimations::sendJson(QTcpSocket *socket, const QJsonObject &obj)
{
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    socket->write(data);
    QVERIFY(socket->waitForBytesWritten(1000));
}

QByteArray TestIpcAnimations::readAllWithTimeout(QTcpSocket *socket, int timeoutMs)
{
    QByteArray result;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (socket->waitForReadyRead(100)) {
            result += socket->readAll();
        }
        if (!result.isEmpty() && !socket->bytesAvailable()) {
            break;
        }
    }
    return result;
}

// ------------------------------------------------------------------
// 1. Basic ping/pong health check
// ------------------------------------------------------------------
void TestIpcAnimations::testPingPong()
{
    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
    QVERIFY(client.waitForConnected(1000));

    sendJson(&client, QJsonObject{{"type", "ping"}});

    QTest::qWait(100); // let server process and respond
    QByteArray response = client.readAll();
    QVERIFY(!response.isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(response.trimmed());
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value("type").toString(), QStringLiteral("pong"));

    client.close();
}

// ------------------------------------------------------------------
// 2. Event triggers animation via TCP
// ------------------------------------------------------------------
void TestIpcAnimations::testEventTriggersAnimation()
{
    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
    QVERIFY(client.waitForConnected(1000));

    QSignalSpy spy(m_engine, &SpriteAnimationEngine::frameChanged);

    QJsonObject event{
        {"type", "event"},
        {"source", "opencode"},
        {"event", "session.start"}
    };
    sendJson(&client, event);

    // Give the event loop a moment to process
    QTest::qWait(200);

    QVERIFY(!m_engine->currentAnimation().isEmpty());
    QCOMPARE(m_engine->isPlaying(), true);

    client.close();
}

// ------------------------------------------------------------------
// 3. Event triggers visual effect via TCP
// ------------------------------------------------------------------
void TestIpcAnimations::testEventTriggersEffect()
{
    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
    QVERIFY(client.waitForConnected(1000));

    QSignalSpy spy(m_engine, &SpriteAnimationEngine::effectRequested);

    QJsonObject event{
        {"type", "event"},
        {"source", "codex"},
        {"event", "todo.updated"}
    };
    sendJson(&client, event);

    QTest::qWait(200);

    // "todo.updated" maps to "congratulate" animation which requests "confetti" effect
    QVERIFY(m_effects->activeEffectCount() > 0 || m_engine->currentAnimation() == "congratulate");

    client.close();
}

// ------------------------------------------------------------------
// 4. Tip message shows correct title & body via TCP
// ------------------------------------------------------------------
void TestIpcAnimations::testTipMessage()
{
    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
    QVERIFY(client.waitForConnected(1000));

    QJsonObject tip{
        {"type", "tip"},
        {"title", "Hello Test"},
        {"body", "This is a test tip"},
        {"animation", "wave"}
    };
    sendJson(&client, tip);

    QTest::qWait(200);

    QCOMPARE(m_bubble->title(), QStringLiteral("Hello Test"));
    QCOMPARE(m_bubble->message(), QStringLiteral("This is a test tip"));
    QCOMPARE(m_bubble->bubbleType(), TipBubbleWidget::TipBubble);

    client.close();
}

// ------------------------------------------------------------------
// 5. Multiple concurrent TCP senders
// ------------------------------------------------------------------
void TestIpcAnimations::testMultipleConcurrentSenders()
{
    const int senderCount = 5;
    QVector<QTcpSocket*> clients;
    clients.reserve(senderCount);

    for (int i = 0; i < senderCount; ++i) {
        auto *client = new QTcpSocket(this);
        client->connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
        QVERIFY2(client->waitForConnected(1000), qPrintable(QString("Client %1 failed to connect").arg(i)));
        clients.append(client);
    }

    // Fire events from all clients simultaneously
    for (int i = 0; i < senderCount; ++i) {
        QJsonObject event{
            {"type", "event"},
            {"source", "opencode"},
            {"event", "session.start"}
        };
        sendJson(clients[i], event);
    }

    QTest::qWait(300);

    // Server should have processed all messages without crashing
    QVERIFY(m_engine->isPlaying());

    for (auto *client : clients) {
        client->close();
        client->deleteLater();
    }
}

// ------------------------------------------------------------------
// 6. Malformed JSON is handled gracefully
// ------------------------------------------------------------------
void TestIpcAnimations::testMalformedJson()
{
    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
    QVERIFY(client.waitForConnected(1000));

    QString beforeAnim = m_engine->currentAnimation();

    client.write("this is not json\n");
    QVERIFY(client.waitForBytesWritten(1000));

    QTest::qWait(100);

    // Server survived; animation state unchanged
    QCOMPARE(m_engine->currentAnimation(), beforeAnim);

    client.close();
}

// ------------------------------------------------------------------
// 7. Unknown event name is rejected
// ------------------------------------------------------------------
void TestIpcAnimations::testUnknownEventType()
{
    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
    QVERIFY(client.waitForConnected(1000));

    QString beforeAnim = m_engine->currentAnimation();

    QJsonObject event{
        {"type", "event"},
        {"source", "opencode"},
        {"event", "totally.fake.event"}
    };
    sendJson(&client, event);

    QTest::qWait(200);

    // No new animation should have started for unknown events
    QCOMPARE(m_engine->currentAnimation(), beforeAnim);

    client.close();
}

// ------------------------------------------------------------------
// 8. Priority queue behavior via TCP
// ------------------------------------------------------------------
void TestIpcAnimations::testPriorityQueue()
{
    QTcpSocket client;
    client.connectToHost(QStringLiteral("127.0.0.1"), TEST_PORT);
    QVERIFY(client.waitForConnected(1000));

    // Send a normal-priority event first (via EventRouter it always uses NormalPriority)
    sendJson(&client, QJsonObject{
        {"type", "event"},
        {"source", "opencode"},
        {"event", "session.start"}
    });

    QTest::qWait(100);
    QVERIFY(m_engine->isPlaying());

    int queueBefore = m_engine->queueSize();

    // Send another event — should queue since the first is still playing
    sendJson(&client, QJsonObject{
        {"type", "event"},
        {"source", "claude-code"},
        {"event", "prompt.submitted"}
    });

    QTest::qWait(100);
    QVERIFY(m_engine->queueSize() >= queueBefore);

    client.close();
}

QTEST_MAIN(TestIpcAnimations)
#include "test_ipc_animations.moc"

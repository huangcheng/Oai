#include "PackDropHandler.h"

#include "CharacterPack.h"
#include "CharacterPackManager.h"
#include "TipWidget.h"
#include "TipsCatalog.h"

#include "../thirdparty/miniz/miniz.h"

#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QStandardPaths>
#include <QUrl>

namespace PackDropHandler {

bool isValidCodexPet(const QString &filePath)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, filePath.toUtf8().constData(), 0)) {
        qWarning() << "PackDropHandler: cannot open ZIP:" << filePath;
        return false;
    }

    int petJsonIdx = mz_zip_reader_locate_file(&zip, "pet.json", nullptr, 0);
    if (petJsonIdx < 0) {
        qWarning() << "PackDropHandler: no pet.json in:" << filePath;
        mz_zip_reader_end(&zip);
        return false;
    }

    size_t petJsonSize = 0;
    void *petJsonData = mz_zip_reader_extract_to_heap(&zip, petJsonIdx, &petJsonSize, 0);
    mz_zip_reader_end(&zip);

    if (!petJsonData) {
        qWarning() << "PackDropHandler: failed to extract pet.json from:" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray(static_cast<const char *>(petJsonData), static_cast<int>(petJsonSize)));
    mz_free(petJsonData);

    if (!doc.isObject()) {
        qWarning() << "PackDropHandler: invalid JSON in pet.json:" << filePath;
        return false;
    }

    const QJsonObject obj = doc.object();
    const QString id = obj.value("id").toString();
    const QString displayName = obj.value("displayName").toString();

    const bool valid = !id.isEmpty() && !displayName.isEmpty();
    if (!valid) {
        qWarning() << "PackDropHandler: missing id or displayName in:" << filePath;
    }
    return valid;
}

void handleDragEnter(QDragEnterEvent *event)
{
    qDebug() << "PackDropHandler: dragEnterEvent";
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.endsWith(".opk", Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
        if ((path.endsWith(".codex-pet", Qt::CaseInsensitive) ||
             path.endsWith(".codex-pet.zip", Qt::CaseInsensitive)) &&
            isValidCodexPet(path)) {
            event->acceptProposedAction();
            return;
        }
    }
    qDebug() << "PackDropHandler: dragEnterEvent ignored";
    event->ignore();
}

namespace {

void handleOpkDrop(const QString &filePath,
                   CharacterPackManager *packManager,
                   TipWidget *tipWidget)
{
    const bool installed = packManager->installPack(filePath);
    const QString msgId = installed
                              ? QStringLiteral("pack.installed")
                              : QStringLiteral("pack.install_failed");
    const auto t = TipsCatalog::instance().message(msgId);
    tipWidget->showBubble(t.title, t.body, TipWidget::TipBubble, "", true);
    qDebug() << "PackDropHandler: .opk install" << (installed ? "succeeded" : "failed");
}

void handleCodexPetDrop(const QString &filePath,
                        CharacterPackManager *packManager,
                        TipWidget *tipWidget)
{
    QFileInfo fi(filePath);
    QString userPacksDir = packManager->userDir();
    if (userPacksDir.isEmpty()) {
        userPacksDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                       + "/packs";
        qWarning() << "PackDropHandler: userDir empty, falling back to" << userPacksDir;
    }
    QDir().mkpath(userPacksDir);

    // fi.fileName() is attacker-controllable via a crafted file:// URL
    // (e.g. "../../Library/LaunchAgents/evil.plist") or a filename that
    // crosses a path separator. Reduce to the base name and reject any
    // residual separator so the destination always stays inside
    // userPacksDir. H13.
    const QString safeName = QFileInfo(fi.fileName()).fileName();
    if (safeName.isEmpty() ||
        safeName.contains('/') || safeName.contains('\\') ||
        safeName == QStringLiteral(".") ||
        safeName == QStringLiteral("..")) {
        qWarning() << "PackDropHandler: rejecting drop with unsafe filename:" << fi.fileName();
        return;
    }
    const QString destFile = userPacksDir + "/" + safeName;

    if (QFile::exists(destFile)) {
        QFile::remove(destFile);
    }
    const bool ok = QFile::copy(filePath, destFile);
    qDebug() << "PackDropHandler: copied .codex-pet to" << destFile << "=" << ok;

    if (ok) {
        const QString builtInDir = packManager->builtInDir();
        const QString currentPackId = packManager->activePackId();
        packManager->initialize(builtInDir, userPacksDir, currentPackId);
        const auto t = TipsCatalog::instance().message(QStringLiteral("pack.installed"));
        tipWidget->showBubble(t.title, t.body, TipWidget::TipBubble, "", true);
    } else {
        const auto t = TipsCatalog::instance().message(QStringLiteral("pack.install_failed"));
        tipWidget->showBubble(t.title, t.body, TipWidget::TipBubble, "", true);
    }
}

} // namespace

void handleDrop(QDropEvent *event,
                CharacterPackManager *packManager,
                TipWidget *tipWidget)
{
    if (!packManager || !tipWidget) {
        qWarning() << "PackDropHandler: dropEvent ignored — missing manager or tip widget";
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    qDebug() << "PackDropHandler: dropEvent with" << urls.size() << "URLs";

    for (const QUrl &url : urls) {
        const QString filePath = url.toLocalFile();
        qDebug() << "PackDropHandler: dropped file:" << filePath;

        if (filePath.endsWith(".opk", Qt::CaseInsensitive)) {
            handleOpkDrop(filePath, packManager, tipWidget);
        } else if ((filePath.endsWith(".codex-pet", Qt::CaseInsensitive) ||
                    filePath.endsWith(".codex-pet.zip", Qt::CaseInsensitive)) &&
                   isValidCodexPet(filePath)) {
            handleCodexPetDrop(filePath, packManager, tipWidget);
        } else {
            qDebug() << "PackDropHandler: dropped file ignored:" << filePath;
        }
    }

    event->acceptProposedAction();
}

} // namespace PackDropHandler

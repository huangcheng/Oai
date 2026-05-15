#include "CharacterPackLoader.h"

#include "../thirdparty/miniz/miniz.h"

#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>

namespace CharacterPackLoader {

namespace {

// Enforce the same 10 MB cap on JSON entries that CharacterPackManager
// uses for installPack / loadFromCodexPet. Anything bigger is either
// adversarial or so misconfigured that we'd rather fail-fast than
// allocate.
constexpr size_t kMaxJsonEntryBytes = 10 * 1024 * 1024;

} // namespace

QJsonObject readJsonEntryFromArchive(const QString &archivePath,
                                      const QString &entryName,
                                      const char *tag)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archivePath.toUtf8().constData(), 0)) {
        qWarning() << tag << ": cannot open archive:" << archivePath;
        return {};
    }

    int idx = mz_zip_reader_locate_file(&zip, entryName.toUtf8().constData(), nullptr, 0);
    if (idx < 0) {
        qWarning() << tag << ": entry" << entryName << "missing in:" << archivePath;
        mz_zip_reader_end(&zip);
        return {};
    }

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(idx), &stat) ||
        stat.m_uncomp_size > kMaxJsonEntryBytes) {
        qWarning() << tag << ":" << entryName << "missing or exceeds size cap in:" << archivePath;
        mz_zip_reader_end(&zip);
        return {};
    }

    size_t size = 0;
    void *data = mz_zip_reader_extract_to_heap(&zip, idx, &size, 0);
    mz_zip_reader_end(&zip);

    if (!data) {
        qWarning() << tag << ": failed to read" << entryName << "from:" << archivePath;
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray(static_cast<const char *>(data), static_cast<int>(size)));
    mz_free(data);

    if (!doc.isObject()) {
        qWarning() << tag << ":" << entryName << "is not a JSON object in:" << archivePath;
        return {};
    }
    return doc.object();
}

bool isValidCodexPet(const QString &archivePath)
{
    const QJsonObject obj = readJsonEntryFromArchive(
        archivePath, QStringLiteral("pet.json"), "CharacterPackLoader::isValidCodexPet");
    if (obj.isEmpty()) return false;
    const QString id = obj.value("id").toString();
    const QString displayName = obj.value("displayName").toString();
    const bool valid = !id.isEmpty() && !displayName.isEmpty();
    if (!valid) {
        qWarning() << "CharacterPackLoader::isValidCodexPet: missing id or displayName in:"
                   << archivePath;
    }
    return valid;
}

QString readOpkPackId(const QString &opkPath)
{
    const QJsonObject obj = readJsonEntryFromArchive(
        opkPath, QStringLiteral("manifest.json"), "CharacterPackLoader::readOpkPackId");
    return obj.value("id").toString();
}

} // namespace CharacterPackLoader

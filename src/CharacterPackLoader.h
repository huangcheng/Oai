#ifndef SEELIE_CHARACTERPACKLOADER_H
#define SEELIE_CHARACTERPACKLOADER_H

#include <QString>
#include <QJsonObject>

/**
 * Free-function helpers for archive-probing logic shared across three sites:
 *   PackDropHandler::isValidCodexPet — drag-acceptance sniff
 *   CharacterPackManager::extractCodexPetInfo — discovery-time metadata
 *   CharacterPack::loadFromCodexPet — actual load
 *
 * Each previously open-coded the same "miniz init → locate → extract heap →
 * parse JSON → mz_zip_reader_end" sequence. Audit H18 wanted a full
 * CharacterPackLoader extraction; the lightweight version here covers the
 * archive-probe duplication without needing to expose CharacterPack
 * internals or refactor every parser to use a builder pattern.
 *
 * The actual `loadFromCodexPet` body stays on CharacterPack (it mutates 8
 * private members) — moving it would require either friend declarations or
 * a builder-pattern rewrite that the audit's "adding a new format" use case
 * doesn't justify.
 */
namespace CharacterPackLoader {

/**
 * Open a ZIP archive, locate `entryName` (e.g. "manifest.json", "pet.json"),
 * read its bytes (size-capped at 10 MB), parse as a JSON object.
 *
 * Returns the parsed object on success, or an empty QJsonObject if the
 * archive can't be opened, the entry is missing, exceeds the size cap,
 * or the contents aren't a JSON object. All failure paths log via qWarning
 * with a `tag` prefix so call sites can identify themselves in the log.
 */
QJsonObject readJsonEntryFromArchive(const QString &archivePath,
                                      const QString &entryName,
                                      const char *tag);

/// Returns true if `archivePath` looks like a valid .codex-pet — pet.json
/// parses cleanly with non-empty id and displayName.
bool isValidCodexPet(const QString &archivePath);

/// Read the `id` field from an .opk archive's manifest.json.
/// Returns empty string on failure.
QString readOpkPackId(const QString &opkPath);

} // namespace CharacterPackLoader

#endif // SEELIE_CHARACTERPACKLOADER_H

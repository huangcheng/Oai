#ifndef SEELIE_PACKDROPHANDLER_H
#define SEELIE_PACKDROPHANDLER_H

#include <QString>

class CharacterPackManager;
class TipWidget;
class QDragEnterEvent;
class QDropEvent;

/**
 * Drag-and-drop install handling for .opk and .codex-pet archives.
 *
 * Extracted from MainWindow (~150 lines) per audit H2 so the drop logic
 * — which doesn't depend on any MainWindow private state beyond the
 * pack manager and the tip widget — can be tested and reasoned about
 * independently. The class is stateless; each call gets a fresh event
 * pointer plus references to the manager and the tip surface.
 *
 * Security: filename sanitization (H13) lives here. Path traversal in
 * a crafted file:// URL is rejected before any QFile::copy.
 *
 * Manifest validation: .codex-pet is sniffed for a valid pet.json
 * before the drag is even accepted, so the cursor changes shape
 * appropriately during a hover.
 */
namespace PackDropHandler {

/// Returns true if `filePath` is a .opk OR a .codex-pet whose pet.json
/// parses cleanly with non-empty id+displayName. Used by both
/// dragEnterEvent (to decide whether to accept the hover) and dropEvent
/// (as a sanity check before the copy).
bool isValidCodexPet(const QString &filePath);

/// Process a drag-enter event. Calls event->acceptProposedAction() if
/// any URL in the mime data looks like an installable pack, else
/// event->ignore().
void handleDragEnter(QDragEnterEvent *event);

/// Process a drop event. Walks the URL list, installs .opk files via
/// the manager, copies .codex-pet files into userDir/, and surfaces
/// success/failure through the tip widget.
///
/// Pre: `packManager` and `tipWidget` are non-null (caller's invariant).
void handleDrop(QDropEvent *event,
                CharacterPackManager *packManager,
                TipWidget *tipWidget);

} // namespace PackDropHandler

#endif // SEELIE_PACKDROPHANDLER_H

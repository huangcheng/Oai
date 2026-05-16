#include "StyleUtils.h"

#include <QStyle>
#include <QWidget>

namespace StyleUtils {

void setVariant(QWidget *w, const char *variant)
{
    if (!w) return;
    w->setProperty("variant", variant);
    // QStyleSheetStyle evaluates rules at polish time. A property assigned
    // after construction (or after the widget is first shown) doesn't trigger
    // re-evaluation on its own — we have to nudge it explicitly. The
    // unpolish/polish pair is the canonical Qt idiom for this.
    if (w->style()) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

} // namespace StyleUtils

#ifndef MACFOCUSFIX_H
#define MACFOCUSFIX_H

#include <QtGlobal>

class QWidget;

namespace MacFocusFix {

// Make this widget's NSWindow non-activating: showing/raising it won't
// activate the app or steal keyboard focus from the user's foreground app.
// No-op on non-macOS platforms.
void makeNonActivating(QWidget *widget);

}

#endif

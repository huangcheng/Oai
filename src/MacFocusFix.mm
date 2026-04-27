#include "MacFocusFix.h"

#include <QWidget>
#include <QWindow>

#ifdef Q_OS_MAC
#import <AppKit/AppKit.h>
#endif

namespace MacFocusFix {

void makeNonActivating(QWidget *widget)
{
#ifdef Q_OS_MAC
    if (!widget)
        return;
    // Force native handle creation so we have an NSView/NSWindow.
    widget->winId();
    QWindow *qwin = widget->windowHandle();
    if (!qwin)
        return;
    NSView *view = reinterpret_cast<NSView *>(qwin->winId());
    if (!view)
        return;
    NSWindow *nswin = [view window];
    if (!nswin)
        return;

    // Add the non-activating panel style mask. This makes the window behave
    // like an NSPanel that never becomes key, so showing or ordering it front
    // does not activate the app or steal keyboard focus from another app.
    NSWindowStyleMask mask = [nswin styleMask];
    mask |= NSWindowStyleMaskNonactivatingPanel;
    [nswin setStyleMask:mask];

    // Belt-and-suspenders: explicitly opt out of becoming key/main.
    // (subclassing isn't possible here, but these properties are honored
    // by AppKit when the underlying window is a panel.)
    [nswin setHidesOnDeactivate:NO];
    // Note: do NOT add NSWindowCollectionBehaviorCanJoinAllSpaces — it
    // conflicts with NSWindowCollectionBehaviorMoveToActiveSpace which Qt
    // sets by default on tool windows, and AppKit raises NSInternalInconsistencyException.
    // Transient + IgnoresCycle alone are enough to stop the bubble from
    // entering the cmd-` cycle or stealing focus.
    [nswin setCollectionBehavior:
        [nswin collectionBehavior]
        | NSWindowCollectionBehaviorTransient
        | NSWindowCollectionBehaviorIgnoresCycle];
#else
    Q_UNUSED(widget);
#endif
}

}

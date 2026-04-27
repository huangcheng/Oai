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
    // AppKit raises NSInternalInconsistencyException when the collection
    // behavior contains conflicting flags. Qt's tool window default sets
    // MoveToActiveSpace + ParticipatesInCycle, which conflict with the
    // CanJoinAllSpaces / IgnoresCycle bits we want for a non-activating
    // bubble. Mask the conflicting bits out *before* OR-ing the new ones.
    NSWindowCollectionBehavior beh = [nswin collectionBehavior];
    beh &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
    beh &= ~NSWindowCollectionBehaviorParticipatesInCycle;
    beh |= NSWindowCollectionBehaviorTransient;
    beh |= NSWindowCollectionBehaviorIgnoresCycle;
    [nswin setCollectionBehavior:beh];
#else
    Q_UNUSED(widget);
#endif
}

}

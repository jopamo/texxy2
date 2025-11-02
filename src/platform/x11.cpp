// src/platform/x11.cpp

#include <QApplication>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include "x11.h"

#include <X11/Xatom.h>

namespace Texxy {

/************************************************************
 *** These are all X11 related functions Texxy uses, ***
 *** because Qt does not fetch enough information on X11. ***
 ************************************************************/

static inline Display* getDisplay() {
#if QT_CONFIG(xcb)
    if (auto x11NativeInterfce = qApp->nativeInterface<QNativeInterface::QX11Application>())
        return x11NativeInterfce->display();
#endif
    return nullptr;
}

// Get the current virtual desktop.
long fromDesktop() {
    long res = -1;

    Display* disp = getDisplay();
    if (!disp)
        return res;

    Atom actual_type;
    int actual_format;
    long unsigned nitems;
    long unsigned bytes;
    long* data = nullptr;
    int status;

    /* QX11Info::appRootWindow() or even RootWindow (disp, 0)
       could be used instead of XDefaultRootWindow (disp) */
    status = XGetWindowProperty(disp, XDefaultRootWindow(disp), XInternAtom(disp, "_NET_CURRENT_DESKTOP", True), 0,
                                (~0L), False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes,
                                (unsigned char**)&data);
    if (status != Success)
        return res;

    if (data) {
        res = *data;
        XFree(data);
    }

    return res;
}
/*************************/
// Get the desktop of a window.
long onWhichDesktop(Window window) {
    long res = -1;

    Display* disp = getDisplay();
    if (!disp)
        return res;

    Atom wm_desktop = XInternAtom(disp, "_NET_WM_DESKTOP", False);
    Atom type_ret;
    int fmt_ret;
    unsigned long nitems_ret;
    unsigned long bytes_after_ret;

    long* desktop = nullptr;

    int status = XGetWindowProperty(disp, window, wm_desktop, 0, 1, False, XA_CARDINAL, &type_ret, &fmt_ret,
                                    &nitems_ret, &bytes_after_ret, (unsigned char**)&desktop);
    if (status != Success)
        return res;
    if (desktop) {
        res = (long)desktop[0];
        XFree(desktop);
    }

    return res;
}
/*************************/
// The following two functions were adapted from x11tools.cpp,
// belonging to kadu (https://github.com/vogel/kadu).
// They are needed because QWidget::isMinimized()
// may not detect the shaded state with all WMs.

bool isWindowShaded(Window window) {
    Display* disp = getDisplay();
    if (!disp)
        return false;

    Atom property = XInternAtom(disp, "_NET_WM_STATE", False);
    if (property == None)
        return false;
    Atom atom = XInternAtom(disp, "_NET_WM_STATE_SHADED", False);
    if (atom == None)
        return false;

    Atom* atoms = nullptr;
    Atom realtype;
    int realformat;
    unsigned long nitems, left;
    int result = XGetWindowProperty(disp, window, property, 0L, 8192L, False, XA_ATOM, &realtype, &realformat, &nitems,
                                    &left, (unsigned char**)&atoms);
    if (result != Success || realtype != XA_ATOM)
        return false;
    for (unsigned long i = 0; i < nitems; i++) {
        if (atoms[i] == atom) {
            XFree(atoms);
            return true;
        }
    }
    XFree(atoms);
    return false;
}
/*************************/
void unshadeWindow(Window window) {
    Display* disp = getDisplay();
    if (!disp)
        return;

    Atom atomtype = XInternAtom(disp, "_NET_WM_STATE", False);
    Atom atommessage = XInternAtom(disp, "_NET_WM_STATE_SHADED", False);
    XEvent xev;
    xev.type = ClientMessage;
    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.window = window;
    xev.xclient.message_type = atomtype;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 0;  // unshade
    xev.xclient.data.l[1] = atommessage;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;
    XSendEvent(disp, DefaultRootWindow(disp), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(disp);
}

}  // namespace Texxy

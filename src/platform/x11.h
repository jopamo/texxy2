// src/platform/x11.h
#ifndef X11_H
#define X11_H

/*
NOTE: This header should be included only if HAS_X11 is true,
      which means that WITHOUT_X11 isn't used with compilation.

      Moreover, the following functions should be called only if
      Texxy is running under X11.
*/

#include <X11/Xlib.h>

namespace Texxy {

long fromDesktop();
long onWhichDesktop(Window window);
bool isWindowShaded(Window window);
void unshadeWindow(Window window);

}  // namespace Texxy

#endif  // X11_H

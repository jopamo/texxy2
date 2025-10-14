/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
 *
 * Texxy is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Texxy is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

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

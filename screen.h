/* 
 * Copyright (C) 2011, David Consuegra
 *
 * This file is part of Haywire
 * 
 * Haywire is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Haywire is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with Haywire.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCREEN_H 
#define SCREEN_H
#include "appstate.h"
#include <curses.h>

//Convert CTRL('A') to the keycode returned by ^A when getch'd
#define CTRL(x) (x - 'A')

void init_screen(haywire_state *app);
void refresh_screen(haywire_state *app);
int list_row_count(haywire_state *app);
void print_error(haywire_state *app, logerror *err, int selected, int row);
#endif

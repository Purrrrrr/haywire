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

#ifndef LINEREADER_H 
#define LINEREADER_H

#define READ_ALL_LINES -1
#include <stdio.h>
#include <stdlib.h>

typedef struct linereader {
  FILE *file;
  char *buffer;
  size_t bufferlength;
} linereader;
linereader *linereader_open(char *filename, int startat);
char *linereader_getline(linereader *r);
void linereader_close(linereader *r);

#endif

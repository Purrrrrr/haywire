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

#ifndef PARSE_LOG_H
#define PARSE_LOG_H

#define E_UNPARSED 0
#define E_ERROR 1
#define E_PARSE 2
#define E_WARNING 4
#define E_NOTICE 8
#define E_MISSING_FILE 256

#define SORT_DEFAULT 1
#define SORT_TYPE 1
#define SORT_TYPE_REVERSE -1
#define SORT_DATE 2
#define SORT_DATE_REVERSE -2
#define SORT_COUNT 3
#define SORT_COUNT_REVERSE -3
#define SORT_MAX 3

typedef struct logerror {
  time_t date;
  short is_new;
  short type;
  int count;
  char *logline; //The whole log line, can contain null characters
  size_t linelength; //The length of the line
  char *msg; //Null terminated log message
  char *filename; //Null terminated filename
  int linenr;
  struct logerror *prev;
  struct logerror *next;
} logerror;

logerror *parse_error_line(const char *line);
int logerror_merge(logerror *this, logerror *that);
int errorlog_cmp(logerror *a, logerror *b, short sorttype);
char *logerror_nicepath(logerror *this, char *relative_to, char **buffer, size_t *n);
void logerror_destroy(logerror *err);

#endif

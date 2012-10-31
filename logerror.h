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

/* The more severe error values are always greater */
#define E_ERROR 1
#define E_PARSE 2
#define E_WARNING 4
#define E_NOTICE 8
#define E_MISSING_FILE 128 
#define E_NOERROR 256
#define E_UNPARSED 256

#define SORT_DEFAULT 1
#define SORT_TYPE 1
#define SORT_TYPE_REVERSE -1
#define SORT_DATE 2
#define SORT_DATE_REVERSE -2
#define SORT_COUNT 3
#define SORT_COUNT_REVERSE -3
#define SORT_MAX 3

typedef struct logerror_occurrence {
  time_t date;
  char *referer;
  char *stack_trace;
  struct logerror_occurrence *prev;
} logerror_occurrence;

typedef struct logerror {
  short is_new;
  short type;
  int count;
  char *key; //A key to identify this message type and origin
  size_t keylength; //The length of the key 

  char *msg; //Null terminated log message
  char *filename; //Null terminated filename
  int linenr;
  
  logerror_occurrence *latest_occurrence;
  
  struct logerror *prev;
  struct logerror *next;
} logerror;

static inline int errortype_worse(short a, short b) {
  return a < b;
}
logerror *parse_error_line(char *line);
int logerror_merge(logerror *this, logerror *that);
int errorlog_cmp(logerror *a, logerror *b, short sorttype);
char *logerror_nicepath(logerror *this, char *relative_to, char **buffer, size_t *n);
void logerror_destroy(logerror *err);

#endif

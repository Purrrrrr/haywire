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

//To get strptime to declare correctly
#define _XOPEN_SOURCE
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>
#include <string.h>

/* A simple line parsing struct that is given a line to parse */
typedef struct lineparser {
  char *line; //Does not really contain about anything now..
} lineparser;

static inline void lineparser_init(lineparser *parser, char *line) {
  parser->line = line;
}
static inline time_t lineparser_read_time(lineparser *parser, char *format) {

  struct tm tm;
  tm.tm_sec = 0;
  tm.tm_min = 0;
  tm.tm_hour = 0;
  tm.tm_mday = 0;
  tm.tm_mon = 0;
  tm.tm_year = 0;
  tm.tm_wday = 0;
  tm.tm_isdst = -1;

  char *line = strptime(parser->line, format, &tm);
  if (line == NULL) return 0;

  parser->line = line;
  
  return mktime(&tm);
}

/* Returns 1 if the current parser begins with beginning */
static inline int lineparser_begins_with(lineparser *parser, char *beginning) {
  char *word = strstr(parser->line, beginning); //Move to first occurrence of beginning
  if (word == parser->line) return 1; //First occurrence at start, we have a match!
  return 0; 
}

/* Returns 1 if the current parser contains the string skip and skips it */
static inline int lineparser_skip_past(lineparser *parser, char *skip) {
  char *word = strstr(parser->line, skip); 
  if (word == NULL) return 0; 
  
  word += strlen(skip);
  parser->line = word;

  return 1; 
}
static inline void lineparser_skip_whitespace(lineparser *parser) {
  char *c = parser->line;
  while(*c != '\0' && !isalnum(*c)) ++c;
  parser->line = c;
}
static inline char *lineparser_read_until(lineparser *parser, char *delim) {
  char *ret = parser->line;
  char *pos = strstr(ret, delim);
  if (pos == NULL) return NULL;

  *pos = '\0';
  pos++;

  parser->line = pos;

  return ret;
}
static inline char *lineparser_read_after_last(lineparser *parser, char *delim) {
  char *next = NULL;
  char *cur = parser->line;

  while(next = strstr(cur+1, delim)) cur = next;
  if (cur == parser->line) return NULL;

  *cur = '\0'; //Prevent using the rest of the string
  cur += strlen(delim);

  return cur;
}
static inline int lineparser_skip_from_end(lineparser *parser, char *skip) {
  char *end = parser->line + strlen(parser->line) - strlen(skip);
  if (strcmp(end, skip) == 0) {
    *end = '\0';
    return 1;
  }

  return 0;
}

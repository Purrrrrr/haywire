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

#ifndef APACHELOGPARSER_H
#define APACHELOGPARSER_H
typedef struct logdata logdata;
typedef char *(*logParser)(char *, logdata *);

typedef struct logParseToken {
  char formatChar;
  char type; //Bool
  char deterministic; //Bool, 1 iff the token can only consume a fixed amount of characters
  char optional; //Bool
  size_t logdatafield; //Which field in the logdata structure to use
  unsigned int string_len_before;
  unsigned int string_len_after;
  char *string_before;
  char *string_after;
  struct logParseToken *next;
  logParser parser;
} logParseToken;

logParseToken *parseLogFormatString(char *fmt);
void destroyLogParser(logParseToken *parser);
char *parseLogLine(logParseToken *parser, char *line, time_t *time, char **referer);

#endif

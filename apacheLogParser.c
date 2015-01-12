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

#include "debug.h"
#include "logerror.h"
#include "apacheLogParser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define TOKEN_STRING 1
#define TOKEN_GARBAGE 2
#define TOKEN_FUNC 3
#define TOKEN_MESSAGE 4



struct logdata {
  time_t date;
  char *messageStart;
  char *messageEnd;
};

char *getText(char **textPosition);
char *parse_format_sign(logParseToken *tok, char *fmt);
char *read_time(char *linepos, logdata *data);
char *read_iso8601_time(char *linepos, logdata *data);
int maxBufSize = 256;


//This is a hack
unsigned int maxTokenBufferSize = 0;

logParseToken *make_token() {
  logParseToken *ret = malloc(sizeof(logParseToken));
  if (ret == NULL) return NULL;

  ret->type = 0;
  ret->optional = 0;
  ret->deterministic = 1;
  ret->string_before = "";
  ret->string_after = "";
  ret->string_len_before = 0;
  ret->string_len_after = 0;
  ret->parser = NULL;
  ret->next = NULL;
  
  maxTokenBufferSize++;

  return ret; 
}

void destroyLogParser(logParseToken *parser) {
  while (parser != NULL) {
    logParseToken *next = parser->next;
    if (parser->string_len_before > 0) {
      free(parser->string_before);
    }
    if (parser->string_len_after > 0) {
      free(parser->string_after);
    }
    free(parser);
    parser = next;
  }
}
logParseToken *parseLogFormatString(char *fmt) {
  if (strlen(fmt) >= maxBufSize) {
    maxBufSize = strlen(fmt) + 1;
  }

  logParseToken *list = NULL;
  logParseToken *cur = NULL;

  while(fmt != NULL && *fmt != '\0') {
    //debug_print("kala: \"%s\"\n", fmt);
    logParseToken *tok = make_token();
    tok->string_before = getText(&fmt);
    //debug_print("tursas: \"%s\"\n", fmt);

    if (tok->string_before == NULL) {
      return NULL;
    }
    tok->string_len_before = strlen(tok->string_before);

    if (*fmt == '%') {
      if (fmt[1] != ' ') {
        fmt = parse_format_sign(tok, fmt);
        //debug_print("fmt: \"%s\"\n", fmt);
        if (fmt == NULL) {
          return NULL;
        }
        if (*fmt != ' ' && *fmt != '%') {
          tok->string_after = getText(&fmt);
          tok->string_len_after = strlen(tok->string_after);
          //debug_print("fmt: \"%s\"\n", fmt);
        }
      } else {
        fmt+=2;
        if (tok->string_len_before) {
          tok->type = TOKEN_STRING;
        }
      }
    } else {
      tok->type = TOKEN_STRING;
    }

    if (tok->type) {
      if (list == NULL) {
        list = cur = tok;
      } else {
        cur->next = tok;
        cur = tok;
      }
    }
  }
  
  return list;
}

char *getText(char **textPosition) {
  char buffer[maxBufSize];
  char *pos = *textPosition;
  int bufPos = 0;
  int at_start = 1;

  while(*pos != '\0' && *pos != '%' && (at_start || *pos != ' ')) {
    char c = *pos;
    if (c == '\\') {
      pos++;
      switch(*pos) {
        case 't': 
          c = '\t';
          break;
        case 'r': 
          c = '\r';
          break;
        case 'n': 
          c = '\n';
          break;
        case '\0': 
          goto end;
        default:
          c = *pos;
      }
    } else {
      if (c != ' ') {
        at_start = 0;
      }
    }
    buffer[bufPos] = c;
    pos++;
    bufPos++;
  }
  
  end:
  buffer[bufPos] = '\0';
  *textPosition = pos;
  return bufPos > 0 ? strdup(buffer) : "";
}
char *parse_format_sign(logParseToken *tok, char *fmt) {
  int can_be_empty = 1;
  char *parameters = NULL;

  if (*fmt != '%') return NULL;
  fmt++; //Skip the %

  //Skip the possible extra modifiers: +, -, and numbers. 
  //This actually eats all these: +,-./0123456789, but it shouldn't matter
  while(*fmt <= '9' && *fmt >= '+') {
    if (*fmt == '-' || *fmt == '+') {
      can_be_empty = 0;
    }
    fmt++;
  }
  //debug_print("possu: \"%s\"\n", fmt);

  //Skip extra modifiers inside {}
  if (*fmt == '{') {
    parameters = ++fmt;
    while(*fmt != '}' && *fmt != '\0') {
      fmt++;
    }
    if (*fmt == '\0') return NULL;
    fmt++;
  }
  //debug_print("possu: \"%s\"\n", fmt);
  if (*fmt == '\0') return NULL;
  tok->formatChar = *fmt;
  fmt++;
  //debug_print("possu: \"%s\"\n", fmt);

  switch(tok->formatChar) {
    case 'M': //The actual log message
      tok->deterministic = 0;
      tok->type = TOKEN_MESSAGE;
      break;
    case 't':
      /* %t  The current time
         %{u}t The current time including micro-seconds
         %{cu}t  The current time in compact ISO 8601 format, including micro-seconds
      */
      tok->type = TOKEN_FUNC;
      tok->parser = &read_time;
      if (parameters != NULL) {
        //I hope somebody never crams more stuff to the parameters
        if (parameters[0] == 'c' || parameters[1] == 'c') {
          tok->parser = &read_iso8601_time;
        }
        //The u parameter is automatically detected by the readers
      }

      break;
    //Handle at least some of the optional format tags.
    //This list is probably not complete
    case 'a':
    case 'e':
    case 'E':
    case 'F':
    case 'i':
    case 'n':
      tok->optional = can_be_empty;
    default:
      tok->deterministic = 0;
      tok->type = TOKEN_GARBAGE;
  }

  return fmt;
}

//TODO: The log parsing always fails is some mad hatter uses a log format like "%{uc}t.12345 %M"
char *read_time(char *linepos, logdata *data) {
  struct tm tm;
  tm.tm_sec = 0;
  tm.tm_min = 0;
  tm.tm_hour = 0;
  tm.tm_mday = 0;
  tm.tm_mon = 0;
  tm.tm_year = 0;
  tm.tm_wday = 0;
  tm.tm_isdst = -1;

  char *line = strptime(linepos, "%a %b %d %T", &tm);
  if (line == NULL) return NULL;

  //We just skip through the microseconds if there are any
  if (*line == '.') {
    line++;
    while(isdigit(*line)) {
      line++;
    }
  }
  //Read the year
  line = strptime(line, " %Y", &tm);
  if (line == NULL) return NULL;

  data->date = mktime(&tm);
  
  return line; 
}

//"2014-12-26 16:55:52.727615"
char *read_iso8601_time(char *linepos, logdata *data) {
  struct tm tm;
  tm.tm_sec = 0;
  tm.tm_min = 0;
  tm.tm_hour = 0;
  tm.tm_mday = 0;
  tm.tm_mon = 0;
  tm.tm_year = 0;
  tm.tm_wday = 0;
  tm.tm_isdst = -1;

  char *line = strptime(linepos, "%Y-%m-%d %T", &tm);
  if (line == NULL) return NULL;

  //We just skip through the microseconds if there are any
  if (*line == '.') {
    line++;
    while(isdigit(*line)) {
      line++;
    }
  }
  data->date = mktime(&tm);
  
  return line; 
}

static inline char *skipString(char *line, char *skip, unsigned int len);

char *advanceParse(logParseToken *current, char *parsePoint, char **parseEndPoint, logdata *data);
char *parseLogToken(logParseToken *parser, char *line, logdata *data);
char *parseLogLine(logParseToken *parser, char *line, time_t *time) {
  logdata data = {
    0, NULL, NULL
  };
  logParseToken startToken = {
    ' ',
    TOKEN_STRING,
    1,
    0,
    "",
    "",
    0,
    0,
    NULL,
    parser 
  };
  char *dummy = NULL;
  
  if (advanceParse(&startToken, line, &dummy, &data) == NULL) return NULL;
  if (data.messageStart == NULL || data.messageEnd == NULL) return NULL;
  if (data.date == 0) return NULL;
  *time = data.date;

  return strndup(data.messageStart, data.messageEnd - data.messageStart);
}

static inline char *skipString(char *line, char *skip, unsigned int len) {
  if (strncmp(line, skip, len) != 0) {
    return NULL;
  }
  return line + len;
}

char *advanceParse(logParseToken *current, char *parsePoint, char **parseEndPoint, logdata *data) {
  logParseToken *next = current->next;
  
  //The strings that need to be found next. 
  //These can be the after string of the current token and the before string of the next one.
  char *matchStrings[2] = {NULL, NULL};
  int matchStringLengths[2] = {0, 0};
  int stringsToAlwaysMatch = 0;

  if (current->string_len_after) {
    matchStrings[0] = current->string_after;
    matchStringLengths[0] = current->string_len_after;
    stringsToAlwaysMatch = 1;
  }
  
  //Loop through all the possible candidates to match next
  while(1) {
    debug_print("Going for the next token (%s%%%c%s), it is %s token\n", 
        next ? next->string_before : "",
        next ? next->formatChar: '0',
        next ? next->string_after: "",
        next ? (next->optional ? "an optional" : "a mandatory") : "a null"
        );


    int stringsToMatch = stringsToAlwaysMatch;
    if (next && next->string_len_before) {
      matchStrings[stringsToMatch] = next->string_before;
      matchStringLengths[stringsToMatch] = next->string_len_before;
      stringsToMatch++;
    }
    for(int s = 0; s < stringsToMatch; s++) {
      debug_print("We need to match \"%s\"\n", matchStrings[s]);
    }
    
    char *curEndPoint = parsePoint;
    while (curEndPoint) {
      debug_print("Seeking from \"%s\"\n", curEndPoint);
      char *searchPosition = curEndPoint;
      int curString = 0;
      if (!current->deterministic) {
        if (stringsToMatch) {
          debug_print("Seeking \"%s\"\n", matchStrings[0]);
          //Seek to the next possible place.
          curEndPoint = strstr(curEndPoint,matchStrings[0]);
          if (!curEndPoint) {
            //Break from the while(curEndPoint) loop
            break; 
          }

          searchPosition = curEndPoint + matchStringLengths[0];
          curString = 1;
        } else if (!next) {
          //There's nothing to match here, we'll just go to the end
          while(*curEndPoint!= '\0') curEndPoint++;
          searchPosition = curEndPoint;
        }
      }
      
      int matched = 1;
      //Loop the (remaining) strings to be matched
      for(int s = curString; s < stringsToMatch; s++) {
        debug_print("Skipping \"%s\" at \"%s\"\n", matchStrings[s], searchPosition);
        searchPosition = skipString(searchPosition, matchStrings[s], matchStringLengths[s]);
        if (!searchPosition) {
          matched = 0;
          break;
        }
      }

      if (matched) {
        debug_print("Matched at \"%s\"\n", searchPosition);
        char *parseResult = next ? parseLogToken(next, searchPosition, data) : searchPosition;
        if (parseResult && *parseResult == '\0') {
          *parseEndPoint = curEndPoint;
          return parseResult;
        }
      }
      if (current->deterministic) break;
      curEndPoint++;
    }

    if (!next) break;
    if (next->optional) {
      next= next->next;
    } else {
      break;
    }
  }
  
  return NULL;
}

char *parseLogToken(logParseToken *parser, char *parsePoint, logdata *data) {
  debug_print("\n\nParsing \"%s%%%c%s\" from \"%s\"\n", parser->string_before, parser->formatChar, parser->string_after, parsePoint);
  if (parsePoint == NULL) return parsePoint;
  debug_print("%d\n", parser->type);
  
  char *end = NULL;
  char *result = NULL;
  switch(parser->type) {
    case TOKEN_STRING:
      return advanceParse(parser, parsePoint, &end, data);
    case TOKEN_FUNC:
      parsePoint = (*parser->parser)(parsePoint, data);
      if (parsePoint) {
        return advanceParse(parser, parsePoint, &end, data);
      }
    case TOKEN_MESSAGE:
    case TOKEN_GARBAGE:
      result = advanceParse(parser, parsePoint, &end, data);
      if (result != NULL) {
        if (parser->type == TOKEN_MESSAGE) {
          data->messageStart = parsePoint;
          data->messageEnd = end;
        }
        return result;
      }
  }
  return NULL;

} 

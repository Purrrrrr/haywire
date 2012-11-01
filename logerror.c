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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "logerror.h"
#include "parser.c"

void parse_php_error(logerror *err, lineparser *parser);
void parse_404(logerror *err, lineparser *parser);
logerror *logerror_init();

logerror *parse_error_line(char *line) {
  lineparser p;
  lineparser_init(&p, line);

  time_t date = lineparser_read_time(&p, "[%a %b %d %T %Y] ");
  if (date == 0) return NULL;
  
  if (!lineparser_skip_past(&p, "[error] [")) return NULL;
  if (!lineparser_skip_past(&p, "] ")) return NULL;

  logerror *err = logerror_init();
  if (err == NULL) return NULL;

  line = strdup(lineparser_current_position(&p));
  lineparser_init(&p, line);

  err->latest_occurrence->date = date;
  err->key = line;
  err->keylength= strlen(line);
  err->msg = err->key;

  if (err->key == NULL) {
    free(err);
    return NULL;
  }

  if (lineparser_begins_with(&p, "PHP")) {
    parse_php_error(err, &p);
  } else if (lineparser_begins_with(&p, "File does not exist")) {
    parse_404(err, &p);
  }
  
  if (err->type == E_UNPARSED) {
    logerror_destroy(err);
    return NULL;
  }

  return err;

}

logerror *logerror_init() {
  logerror *err = malloc(sizeof(logerror));
  if (err == NULL) return NULL;

  logerror_occurrence *occ = malloc(sizeof(logerror_occurrence));
  if (occ == NULL) {
    free(err);
    return NULL;
  }

  occ->date = 0;
  occ->referer = NULL;
  occ->stack_trace = NULL;
  occ->prev = NULL;

  err->count = 1;
  err->is_new = 1;
  err->key = NULL;
  err->keylength = 0;
  err->msg = NULL;
  err->filename = NULL;
  err->linenr = 0;
  err->type = E_UNPARSED;
  
  err->latest_occurrence = occ;
  err->prev = NULL;
  err->next = NULL;

  return err;
}

void logerror_destroy(logerror *err) {
  logerror_occurrence *occ = err->latest_occurrence;
  logerror_occurrence *tmp;

  while(occ != NULL) {
    if (occ->referer != NULL) free(occ->referer);
    if (occ->stack_trace != NULL) free(occ->stack_trace);
    tmp = occ->prev;
    free(occ);
    occ = tmp;
  }

  free(err->key);
  free(err);
}

//Returns:
// -1 if a < b
//  1 if a > b
//  0 if a == b
int errorlog_cmp(logerror *a, logerror *b, short sorttype) {
  //Deleted entries always go to the bottom
  if (a->count == 0) return -1;
  if (b->count == 0) return 1;
  switch(sorttype) {
    case SORT_DATE:
      break; //Handled below
    case SORT_DATE_REVERSE:
      if (a->latest_occurrence->date > b->latest_occurrence->date) return 1;
      if (a->latest_occurrence->date < b->latest_occurrence->date) return -1;
      return 0;
    case SORT_COUNT:
      if (a->count < b->count) return 1;
      if (a->count > b->count) return -1;
      break;
    case SORT_COUNT_REVERSE:
      if (a->count < b->count) return -1;
      if (a->count > b->count) return 1;
      break;
    case SORT_TYPE_REVERSE:
      if (a->type < b->type) return 1;
      if (a->type > b->type) return -1;
      break;
    default:
      //case SORT_TYPE:
      //The most small numbered types are the most important
      if (a->type < b->type) return -1;
      if (a->type > b->type) return 1;
  }
  
  //The newest logs are shown first
  if (a->latest_occurrence->date > b->latest_occurrence->date) return -1;
  if (a->latest_occurrence->date < b->latest_occurrence->date) return 1;

  //There could be more sort criteria, but those two almost always differ anyway.
  return 0;
}

//Merges two similar log entries. Frees the second argument if succesful.
int logerror_merge(logerror *this, logerror *that) {
  //Make sure the errors are the same:
  if (this->keylength != that->keylength) return 0;
  if (memcmp(this->key, that->key, this->keylength) != 0) return 0;

  this->count += that->count;
  
  //Take occurrence lists and destroy that
  logerror_occurrence *list1 = this->latest_occurrence;
  logerror_occurrence *list2 = that->latest_occurrence;
  that->latest_occurrence = NULL;
  logerror_destroy(that);

  /*
  if (that->date > this->date) this->date = that->date; */

  if (list1 == NULL) {
    this->latest_occurrence = list2;
    return 1;
  }
  if (list2 == NULL) return 1; 

  logerror_occurrence *cur = NULL;
  logerror_occurrence *later = NULL;

  while(list1 != NULL && list2 != NULL) {
    
    later = list1->date > list2->date ? list1 : list2;

    if (cur == NULL) {
      this->latest_occurrence = cur = later;
    } else {
      cur->prev = later;
      cur = later;
    }
    if (later == list1) {
      list1 = later->prev;
    } else {
      list2 = later->prev;
    }
    cur->prev= NULL;
  }
  if (list1 != NULL) {
    cur->prev = list1;
  } else if (list2 != NULL) {
    cur->prev = list2;
  }

  return 1;
}

char *logerror_nicepath(logerror *this, char *relative_to, char **buffer, size_t *n) {
  char *path = this->filename;
  if (path == NULL) return NULL;
  int i = 1;
  int ups = 0;

  while(*relative_to != '\0' && *path != '\0') {
    while(relative_to[i] != '/' && relative_to[i] != '\0') ++i;

    if (strncmp(path, relative_to, i-1) != 0) {
      ++ups;
      while(relative_to[i] != '\0') { 
        if (relative_to[i] == '/') ++ups;
        ++i;
      }
      break;
    }

    //The current paths are same
    path += i;
    relative_to += i;
    i = 1;

  }
  int len = strlen(path) + ups*3 + 1;
  if (len > *n) {
    while(len > *n) *n *= 2;
    *buffer = realloc(*buffer, *n);
  }
  int pos = 0;
  while(ups) {
    strncpy(*buffer + pos, "../", 3);
    pos += 3;
    --ups;
  }
  strcpy(*buffer + pos, path + 1);

  return path;
}


/*
 [Mon Apr 18 11:21:27 2011] [error] [client 89.27.5.98] PHP Fatal error:  Uncaught exception 'Exception' with message 'Virhe tallennettaessa asiakastietoja' in /var/www/matkapojat-uusi/web/libraries/matkapojat/db/winres.php:623\nStack trace:\n#0 /var/www/matkapojat-uusi/web/libraries/matkapojat/db/testi2.php(37): WinresAsiakas->store()\n#1 {main}\n  thrown in /var/www/matkapojat-uusi/web/libraries/matkapojat/db/winres.php on line 623
 [Mon Apr 18 11:22:39 2011] [error] [client 194.142.151.79] PHP Notice:  Undefined property: JView::$lahdot_left in /var/www/matkapojat-uusi/web/templates/matkapojat/modules/frontpage_list/frontpage_list.html.php on line 21, referer: http://matkapojatd.ath.cx/kiertomatkat/puola
 */

void parse_php_error(logerror *err, lineparser *p) {
  lineparser_skip_past(p, "PHP ");

  char *typestring = lineparser_read_until(p, ":");

  if (   strcmp(typestring, "Catchable fatal error") == 0 
      || strcmp(typestring, "Fatal error") == 0) 
  {
    err->type = E_ERROR;
  } else if (strcmp(typestring, "Parse error") == 0) {
    err->type = E_PARSE;
  } else if (strcmp(typestring, "Warning") == 0) {
    err->type = E_WARNING;
  } else if (strcmp(typestring, "Notice") == 0) {
    err->type = E_NOTICE;
  } else {
    return;
  }

  lineparser_skip_whitespace(p);
  
  //Get filename, line number and referer from the end of the string
  //in reverse order off course.
  char *referer = lineparser_read_after_last(p, "referer: ");
  char *linenr = lineparser_read_after_last(p, " on line ");
  err->filename = lineparser_read_after_last(p, " in ");
  if (lineparser_begins_with(p, "Uncaught exception")) {
    err->msg = lineparser_read_until(p, " in ");
    lineparser_skip_past(p, "Stack trace:\\n");
    char *stack_trace = lineparser_read_until(p, " thrown");
    if (stack_trace != NULL) {
      char *newline = NULL;
      while((newline = strstr(stack_trace, "\\n")) != NULL) {
        newline[0] = ' ';
        newline[1] = '\n';
      }

      //Move to end of line and trim
      char *c = stack_trace;
      while(*(++c) != '\0');
      while(isspace(*(--c))) *c = '\0';

      err->latest_occurrence->stack_trace = strdup(stack_trace);

    }
  } else {
    err->msg = lineparser_current_position(p);
  }

  if (linenr == NULL || err->filename == NULL) {
    err->type = E_UNPARSED;
    return;
  }
  if (referer != NULL) {
    err->keylength -= strlen(referer) + strlen("referer: ");
    err->latest_occurrence->referer = strdup(referer);
  }
  if (!sscanf(linenr, "%d", &(err->linenr))) {
    err->type = E_UNPARSED;
    return;
  }
}
void parse_404(logerror *err, lineparser *parser) {
  err->type = E_MISSING_FILE;
  err->msg = err->key;

  char *filename = strstr(err->key, ":");
  *filename = '\0'; //Null terminate the message
  filename += 2; //Forward past the ": "
  err->filename = filename;
  err->linenr = 0;
}

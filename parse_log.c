//To get strptime to declare correctly
#define _XOPEN_SOURCE
#define __USE_XOPEN
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "parse_log.h"

void parse_php_error(logerror *err);
void parse_404(logerror *err);

logerror *parse_error_line(const char *line) {

  struct tm tm;
  tm.tm_sec = 0;
  tm.tm_min = 0;
  tm.tm_hour = 0;
  tm.tm_mday = 0;
  tm.tm_mon = 0;
  tm.tm_year = 0;
  tm.tm_wday = 0;
  tm.tm_isdst = -1;

  line = strptime(line, "[%a %b %d %T %Y] ", &tm);
  if (line == NULL) return NULL;

  char *error = NULL;
  
  error = strstr(line, "[error] [");
  if (error == NULL) return NULL;
  error += 9; //strlen("[error] [");

  error = strstr(error, "] ");
  if (error == NULL) return NULL;
  error += 2;

  logerror *err = malloc(sizeof(logerror));
  if (err == NULL) return NULL;

  err->count = 1;
  err->is_new = 1;
  err->date = mktime(&tm);
  err->logline = strdup(error);
  err->linelength = strlen(error);
  err->msg = err->logline;
  err->filename = err->logline;
  err->linenr = 0;
  err->type = E_UNPARSED;
  err->prev = NULL;
  err->next = NULL;

  if (err->logline == NULL) {
    free(err);
    return NULL;
  }

  char *phperror = strstr(line, "PHP");
  if (phperror == error) parse_php_error(err);
  else {
    char *e404_str = strstr(line, "File does not exist");
    if (e404_str == error) parse_404(err);
  }
  
  if (err->type == E_UNPARSED) {
    logerror_destroy(err);
    return NULL;
  }

  return err;

}

void logerror_destroy(logerror *err) {
  free(err->logline);
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
      if (a->date > b->date) return 1;
      if (a->date < b->date) return -1;
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
  if (a->date > b->date) return -1;
  if (a->date < b->date) return 1;

  //There could be more sort criteria, but those two almost always differ anyway.
  return 0;
}

//Merges two similar log entries. Frees the second argument if succesful.
int logerror_merge(logerror *this, logerror *that) {
  //Make sure the errors are the same:
  if (this->linelength != that->linelength) return 0;
  if (memcmp(this->logline, that->logline, this->linelength) != 0) return 0;

  if (that->date > this->date) this->date = that->date;
  this->count += that->count;

  logerror_destroy(that);
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

static const char *filename_prefix = " in ";
static const char *filename_prefix_exceptions = " thrown";
static const char *linenr_prefix = " on line ";

void parse_php_error(logerror *err) {
  char *line = err->logline + 4; //Get past "PHP "
  char *msg = strstr(line,":");
  *msg = '\0'; //Temporarily blank the ':';

  if (strcmp(line, "Catchable fatal error") == 0 || strcmp(line, "Fatal error") == 0) {
    err->type = E_ERROR;
  } else if (strcmp(line, "Parse error") == 0) {
    err->type = E_PARSE;
  } else if (strcmp(line, "Warning") == 0) {
    err->type = E_WARNING;
  } else if (strcmp(line, "Notice") == 0) {
    err->type = E_NOTICE;
  } else {
    return;
  }

  //*msg = ':'; //Restore the ':';
  while(msg != '\0' && !isalnum(*msg)) ++msg;
  err->msg = msg;

  char *filename = msg; 
  char *next = NULL;
  while(next = strstr(filename+1, filename_prefix)) filename = next;

  if (filename == msg) {
    err->type = E_UNPARSED;
    return;
  }
  
  char *test = filename-strlen(filename_prefix_exceptions);
  if (strstr(test, filename_prefix_exceptions) == test) filename = test;

  *filename = '\0'; //End the msg string. 
  err->filename = filename + strlen(filename_prefix);

  char *linenr = strstr(filename + 4, linenr_prefix);
  if (linenr == NULL) {
    err->type = E_UNPARSED;
    return;
  }

  *linenr = '\0';
  linenr += strlen(linenr_prefix);
  if (!sscanf(linenr, "%d", &(err->linenr))) {
    err->type = E_UNPARSED;
    return;
  }

}
void parse_404(logerror *err) {
  err->type = E_MISSING_FILE;
  
  err->msg = err->logline;
  char *filename = strstr(err->logline, ":");
  *filename = '\0'; //Null terminate the message
  filename += 2; //Forward past the ": "
  err->filename = filename;
  err->linenr = -1;
}

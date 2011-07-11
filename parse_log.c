#include "logs.h"

//To get strptime to declare correctly
#define _XOPEN_SOURCE
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>
#include <string.h>

void parse_php_error(logerror *err);
void parse_404(logerror *err);
void logerror_destroy(logerror *err);

logerror *parse_error_line(const char *line) {

  struct tm tm;
  line = strptime(line, "[%a %b %d %T %Y] ", &tm);
  if (line == NULL) return NULL;

  char *error = NULL;
  
  error = strstr(line, "[error] [");
  if (error == NULL) return NULL;
  error = strstr(line, "] ");
  if (error == NULL) return NULL;

  logerror *err = malloc(sizeof(logerror));
  if (err == NULL) return NULL;

  err->count = 1;
  err->linenr = 0;
  err->date = mktime(&tm);
  err->logline = strdup(error);
  err->linelength = strlen(error);
  err->type = E_UNPARSED;

  char *phperror = strstr(line, "PHP");
  if (phperror == error) parse_php_error(err);
  else {
    char *e404_str = strstr(line, "File does not exist");
    if (e404_str == error) parse_404(err);
  }
  
  if (err->type == E_UNPARSED) {
    printf("%s\n", err->logline);
    free(err);
    return NULL;
  }

  return err;

}

void parse_php_error(logerror *err) {
}
void parse_404(logerror *err) {
  err->type = E_MISSING_FILE;
  
  err->msg = err->logline;
  char *filename = strstr(err->logline, ":");
  filename = '\0'; //Null terminate the message
  filename += 2; //Forward past the ": "
  err->filename = filename;
  err->linenr = -1;
}

void logerror_destroy(logerror *err) {
  free(err->logline);
  free(err);
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

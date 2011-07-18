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
#define SORT_MAX 2

typedef struct logerror {
  time_t date;
  short is_new;
  int type;
  int count;
  char *logline; //The whole log line, can contain null characters
  size_t linelength; //The length of the line
  char *msg; //Null terminated log message
  char *filename; //Null terminated filename
  int linenr;
  struct logerror *next;
} logerror;

logerror *parse_error_line(const char *line);
int logerror_merge(logerror *this, logerror *that);
int errorlog_cmp(logerror *a, logerror *b, short sorttype);
char *logerror_nicepath(logerror *this, char *relative_to, char **buffer, size_t *n);
void logerror_destroy(logerror *err);

#endif

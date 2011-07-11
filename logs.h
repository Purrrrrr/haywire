#ifndef LOGS_H
#define LOGS_H

#include <stdio.h>
#include "ghthash/ght_hash_table.h"

#define READ_ALL_LINES -1

#define E_UNPARSED 0
#define E_ERROR 1
#define E_WARNING 2
#define E_PARSE 4
#define E_NOTICE 8
#define E_MISSING_FILE 256

typedef struct logfile {
  FILE *file;
  ght_hash_table_t *errortypes;
  unsigned int empty_entries;
} logfile;

typedef struct logerror {
  time_t date;
  int type;
  int count;
  char *logline; //The whole log line, can contain null characters
  size_t linelength; //The length of the line
  char *msg; //Null terminated log message
  char *filename; //Null terminated filename
  int linenr;
} logerror;

//Opens a log file and digs and sorts the log entries
logfile *logfile_create();
int logfile_add_file(logfile *log, char *filename, int startat);
//Tests for new entries and stores them
int logfile_refresh(logfile *log);
void logfile_close(logfile *log);

logerror **logfile_get_errors(logfile *log);
//Removes the error from the log file log. Assumes err is from log.
void logfile_remove_error(logfile *log, logerror *err);

#endif

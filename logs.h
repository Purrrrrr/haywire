#ifndef LOGS_H
#define LOGS_H

#include <stdio.h>
#include "ghthash/ght_hash_table.h"
#include "parse_log.h"

#define READ_ALL_LINES -1

typedef struct logfile {
  FILE *file;
  ght_hash_table_t *errortypes;
  logerror *errorlist;
  unsigned int empty_entries;
  unsigned long inspected_lines;
} logfile;

//Opens a log file and digs and sorts the log entries
logfile *logfile_create();
int logfile_add_file(logfile *log, char *filename, int startat);
//Tests for new entries and stores them
int logfile_refresh(logfile *log);
logerror *errorlist_sort(logfile *log, int sorttype);
int errorlist_count(logfile *log);
void logfile_close(logfile *log);

//Removes the error from the log file log. Assumes err is from log.
void logfile_remove_error(logfile *log, logerror *err);

#endif

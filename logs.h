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
  short worst_new_line; //Stores the worst type of recent new lines
  short worst_new_type; //Stores the worst type of recent new errors
                        //that have not been encountered before
  short sorting;
  unsigned int empty_entries;
} logfile;

//Opens a log file and digs and sorts the log entries
logfile *logfile_create();
int logfile_add_file(logfile *log, char *filename, int startat);
//Tests for new entries and stores them
int logfile_refresh(logfile *log);
int errorlist_count(logfile *log);
void logfile_close(logfile *log);

//Removes the error from the log file log. Assumes err is from log.
void logfile_remove_error(logfile *log, logerror *err);

void toggle_sort_type(logfile *log);
void toggle_sort_direction(logfile *log);

#endif

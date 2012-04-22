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

#ifndef LOGS_H
#define LOGS_H

#include "ghthash/ght_hash_table.h"
#include "linereader.h"
#include "parse_log.h"

typedef struct logfile {
  linereader *file;
  ght_hash_table_t *errortypes;
  logerror *errorlist;
  short worstNewLine; //Stores the worst type of recent new lines
  short worstNewType; //Stores the worst type of recent new errors
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

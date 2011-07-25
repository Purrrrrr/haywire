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

#include "logs.h"
#include "parse_log.h"
#define SEEK_BUFFER_SIZE 1024

//Helper functions
void logfile_seek(FILE *logfile, int initial_row_count);
void errorlist_remove_error(logerror *err);
logerror *errorlist_insert(logerror *list, logerror *ins);
logerror *errorlist_merge(logerror *list1, logerror *list2, int sorttype);
logerror *errorlist_sort(logerror *list, int sorttype);

logfile *logfile_create() {
  logfile *log;
  log = malloc(sizeof(logfile));
  log->file = NULL;

  if (log == NULL) return NULL;

  ght_hash_table_t *map = ght_create(256);
  if (map == NULL) {
    free(log);
    return NULL;
  }
  log->errortypes = map;
  log->errorlist = NULL;
  log->sorting = SORT_DEFAULT;
  log->worst_new_line = 0;
  log->worst_new_type = 0;

  return log;
}

int logfile_add_file(logfile *log, char *filename, int initial_row_count) {
  FILE *logfile = NULL;
  
  logfile = fopen(filename, "r");
  if (logfile == NULL) return 0;
  logfile_seek(logfile, initial_row_count);

  if (log->file != NULL) fclose(log->file);
  log->file = logfile;
  logfile_refresh(log);
  
  return 1;
}

void logfile_seek(FILE *logfile, int initial_row_count) {
  if (initial_row_count == READ_ALL_LINES) {
    fseeko(logfile, 0, SEEK_SET);
    return;
  }
  fseeko(logfile, 0, SEEK_END);
  if (initial_row_count == 0) return;

  long pos_in_buffer = 0;
  int row_count = 0;
  char *buffer;
  buffer = malloc(sizeof(char) * SEEK_BUFFER_SIZE);

  //Rewind a bit
  int read;
  off_t buffer_pos = ftello(logfile);
  if (buffer_pos == 0) return;
  off_t seek = buffer_pos % SEEK_BUFFER_SIZE;
  buffer_pos -= seek != 0 ? seek : SEEK_BUFFER_SIZE;
  fseeko(logfile, buffer_pos, SEEK_SET);

  while((read = fread(buffer, sizeof(char), SEEK_BUFFER_SIZE, logfile)) > 0) {

    for(pos_in_buffer = read-1; pos_in_buffer >= 0; --pos_in_buffer) {
      if (buffer[pos_in_buffer] != '\n') continue;
      ++row_count;
      if (row_count > initial_row_count) goto end; //The same as two breaks
    }

    if (buffer_pos == 0) {
      break;
    } else {
      buffer_pos -= SEEK_BUFFER_SIZE;
      fseeko(logfile, buffer_pos, SEEK_SET);
    }
  }
  
  //We goto here from the for loop that inspects the buffer.
  end:

  //If not enough rows, then the file is already seeked properly to the beginning
  if (row_count <= initial_row_count) {
    fseeko(logfile, 0, SEEK_SET);
  }
  
  fseeko(logfile, buffer_pos+pos_in_buffer+1, SEEK_SET);
}

int logfile_refresh(logfile *log) {
  int rowcount = 0;
  char *buffer = NULL;
  size_t n = 0;
  ssize_t len = 0;
  logerror *err;
  logerror *err_in_table;
  logerror *newlist = NULL;

  while(len = getline(&buffer, &n, log->file)) {
    if (len < 0) break;
    //We don't like 'em trailin newlines!
    if (len >= 1 && buffer[len-1] == '\n') buffer[len-1] = '\0';

    err = parse_error_line(buffer);
    if (err == NULL) continue;
    if (!log->worst_new_line || log->worst_new_line > err->type) log->worst_new_line = err->type;
    
    //Get a possible duplicate entry in the hashtable
    err_in_table = ght_get(log->errortypes, err->linelength, err->logline);
    if (err_in_table == NULL) {
      ght_insert(log->errortypes, err, err->linelength, err->logline);
      newlist = errorlist_insert(newlist, err);
      if (!log->worst_new_type || log->worst_new_type > err->type) log->worst_new_type = err->type;
    } else {
      logerror_merge(err_in_table, err);
      if (err_in_table->is_new == 0) {
        errorlist_remove_error(err_in_table);
        newlist = errorlist_insert(newlist, err_in_table);
      }
      err_in_table->is_new = 1;
    }
    ++rowcount;
  }
  if (newlist != NULL) {
    newlist = errorlist_sort(newlist, log->sorting);
    //printf("Sorted, final merge");
    log->errorlist = errorlist_merge(log->errorlist, newlist, log->sorting);
  }
  if (buffer != NULL) free(buffer);

  return rowcount;
}

void logfile_close(logfile *log) {
  ght_iterator_t iterator;
  const void *p_key;
  void *p_e;
  for(p_e = ght_first(log->errortypes, &iterator, &p_key); p_e; p_e = ght_next(log->errortypes, &iterator, &p_key))
  {
    logerror_destroy((logerror *)p_e);
  }
  ght_finalize(log->errortypes);

  fclose(log->file);
  free(log);
}

int errorlist_count(logfile *log) {
  return ght_size(log->errortypes) - log->empty_entries;
} 
logerror *errorlist_insert(logerror *list, logerror *ins) {
  //printf("Insert %d\n", ins);
  if (list != NULL) {
    list->prev = ins;
  }
  ins->prev = NULL;
  ins->next = list;
  return ins;
}
void errorlist_remove_error(logerror *err) {
  //printf("Remove %d\n", err);
  if (err->prev != NULL) {
    err->prev->next = err->next;
  }
  if (err->next != NULL) {
    err->next->prev= err->prev;
  }
}
logerror *errorlist_merge(logerror *list1, logerror *list2, int sorttype) {
  logerror *cur = NULL;
  logerror *list = NULL;
  logerror *lesser = NULL;
  //printf("Merge\n");

  if (list1 == NULL) return list2;
  if (list2 == NULL) return list1; 

  while(list1 != NULL && list2 != NULL) {

    if (errorlog_cmp(list1, list2, sorttype) < 0) { //list1 < list2
      lesser = list1;
    } else {
      lesser = list2;
    }

    if (cur == NULL) {
      list = cur = lesser;
    } else {
      cur->next = lesser;
      lesser->prev = cur;
      cur = lesser;
    }
    if (lesser == list1) {
      list1 = lesser->next;
    } else {
      list2 = lesser->next;
    }
    lesser->next = NULL;
  }
  if (list1 != NULL) {
    cur->next = list1;
    list1->prev = cur;
  } else if (list2 != NULL) {
    cur->next = list2;
    list2->prev = cur;
  }

  return list;
}
logerror *errorlist_sort(logerror *err, int sorttype) {
  if (err->next == NULL) return err;
  //printf("Sort! %d\n", err);

  logerror *middle = err;
  logerror *last = err;
  while(last != NULL) {
    middle = middle->next;
    last = last->next;
    if (last != NULL) last = last->next;
  }
  middle->prev->next = NULL;  

  err = errorlist_sort(err, sorttype);
  middle = errorlist_sort(middle, sorttype);

  return errorlist_merge(err, middle, sorttype);
} 

void logfile_remove_error(logfile *log, logerror *err) {
  ++log->empty_entries;
  err->count = 0;
}

//Ordering functions
void toggle_sort_direction(logfile *log) {
  log->sorting = -log->sorting;
  log->errorlist = errorlist_sort(log->errorlist, log->sorting);
}
void toggle_sort_type(logfile *log) {
  short sort = log->sorting;
  if (sort > 0) {
    ++sort;
    if (sort > SORT_MAX) sort = 1;
  } else {
    --sort;
    if (sort < -SORT_MAX) sort = -1;
  }
  log->sorting = sort;
  log->errorlist = errorlist_sort(log->errorlist, sort);
}

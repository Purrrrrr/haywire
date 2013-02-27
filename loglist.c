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

#include "loglist.h"
#include "logerror.h"

//These two are compared to the results of the filter function 
#define FILTER_REMOVE_FAILED FILTER_FAIL
#define FILTER_REMOVE_PASSED FILTER_PASS

//Helper functions
static void update_top_errortype(short *top, logerror *err);
logerror *errorlist_remove_error(logerror *list, logerror *err);
logerror *errorlist_insert(logerror *list, logerror *ins);
logerror *errorlist_merge(logerror *list1, logerror *list2, int sorttype);
logerror *errorlist_sort(logerror *list, int sorttype);
unsigned int loglist_filter(logerror **list, logerror **filtered, logfilter filter, void *data, unsigned short int );

logfile *logfile_create() {
  logfile *log;
  log = malloc(sizeof(logfile));
  if (log == NULL) return NULL;

  ght_hash_table_t *map = ght_create(256);
  if (map == NULL) {
    free(log);
    return NULL;
  }
  log->file = NULL;
  log->filter = NULL;
  log->filter_data = NULL;
  log->errortypes = map;
  log->errors = NULL;
  log->filtered_errors = NULL;
  log->sorting = SORT_DEFAULT;
  log->worstNewLine = 0;
  log->worstNewType = 0;
  log->filtered_entries = 0;
  log->empty_entries = 0;

  return log;
}

int logfile_add_file(logfile *log, char *filename, int initial_row_count) {
  log->file = linereader_open(filename, initial_row_count);
  if (log->file == NULL) {
    return 0; 
  }

  logfile_refresh(log);
  return 1;
}

int logfile_refresh(logfile *log) {
  int rowcount = 0;
  char *line = NULL;
  logerror *err;
  logerror *err_in_table;
  logerror *newlist = NULL;
  logerror *newlist_filtered  = NULL;
  unsigned int filtered_count = 0;

  while((line = linereader_getline(log->file)) != NULL) {
    err = parse_error_line(line);
    if (err == NULL) continue;

    update_top_errortype(&log->worstNewLine, err);
    
    //Get a possible duplicate entry in the hashtable
    err_in_table = ght_get(log->errortypes, err->keylength, err->key);

    if (err_in_table == NULL) {
      //Insert a completely new error
      ght_insert(log->errortypes, err, err->keylength, err->key);
      newlist = errorlist_insert(newlist, err);

      update_top_errortype(&log->worstNewType, err);
    } else {
      //Merge the two duplicate errors
      logerror_merge(err_in_table, err);

      if (err_in_table->is_new == 0) {
        log->errors = errorlist_remove_error(log->errors, err_in_table);
        newlist = errorlist_insert(newlist, err_in_table);
      }

      err_in_table->is_new = 1;
    }
    ++rowcount;
  }
  if (newlist != NULL) {
    //First we reset the is_new bits so that next refreshes work correctly
    err = newlist;
    while (err != NULL) {
      err->is_new = 0;
      err = err->next;
    }
    
    if (log->filter) {
      filtered_count = loglist_filter(&newlist, &newlist_filtered, log->filter, log->filter_data, FILTER_REMOVE_FAILED);

      newlist_filtered = errorlist_sort(newlist_filtered, log->sorting);
    }

    newlist = errorlist_sort(newlist, log->sorting);
    //printf("Sorted, final merge");
    
    log->errors = errorlist_merge(log->errors, newlist, log->sorting);
    log->filtered_errors = errorlist_merge(log->filtered_errors, newlist_filtered, log->sorting);
    log->filtered_entries += filtered_count;
  }

  return rowcount;
}

void logfile_clear(logfile *log) {
  ght_iterator_t iterator;
  const void *p_key;
  void *p_e;
  for(p_e = ght_first(log->errortypes, &iterator, &p_key); p_e; p_e = ght_next(log->errortypes, &iterator, &p_key))
  {
    logerror_destroy((logerror *)p_e);

  }
  ght_finalize(log->errortypes);

  ght_hash_table_t *map = ght_create(256);
  if (map == NULL) {
    free(log);
    exit(EXIT_FAILURE);
  }
  log->errortypes = map;
  log->errors = NULL;
  log->filtered_errors = NULL;
  log->worstNewLine = 0;
  log->worstNewType = 0;
  log->filtered_entries = 0;
  log->empty_entries = 0;

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
  
  linereader_close(log->file);
  free(log);
}

int errorlist_count(logfile *log) {
  return ght_size(log->errortypes) - log->empty_entries - log->filtered_entries;
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
logerror *errorlist_remove_error(logerror *list, logerror *err) {
  if (list == err) list = list->next;
  //printf("Remove %d\n", err);
  if (err->prev != NULL) {
    err->prev->next = err->next;
  }
  if (err->next != NULL) {
    err->next->prev= err->prev;
  }
  return list;
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
  middle->prev = NULL;

  err = errorlist_sort(err, sorttype);
  middle = errorlist_sort(middle, sorttype);

  return errorlist_merge(err, middle, sorttype);
} 

void logfile_remove_error(logfile *log, logerror *err) {
  ++log->empty_entries;
  err->count = 0;
}

//Filtering functions
void logfile_set_filter(logfile *log, logfilter filter, void *data) {
  log->filter = filter;
  log->filter_data = data;

  if (filter == NULL) {
    log->errors = errorlist_merge(log->errors, log->filtered_errors, log->sorting);
    log->filtered_errors = NULL;
    log->filtered_entries = 0;
    return;
  }

  logerror *weeded = NULL;
  logerror *passing = NULL;
  int weeded_count = 0;
  int passed_count = 0;

  weeded_count = loglist_filter(&(log->errors), &weeded, filter, data, FILTER_REMOVE_FAILED);
  passed_count = loglist_filter(&(log->filtered_errors), &passing, filter, data, FILTER_REMOVE_PASSED);

  log->errors = errorlist_merge(log->errors, passing, log->sorting);
  log->filtered_errors = errorlist_merge(log->filtered_errors, weeded, log->sorting);

  log->filtered_entries = log->filtered_entries + weeded_count - passed_count;
}

/* Searches the first list and filters to the second according to the given filter and mode
 * The mode affects whether to move the items that pass or those that don't
 * Returns the number of items in the second list after the update.
 */
unsigned int loglist_filter(logerror **list, logerror **filtered, logfilter filter, void *data, unsigned short int mode) {
  logerror *local_list = *list;
  logerror *local_filtered = NULL;
  logerror *last_failed = NULL;
  logerror *next = NULL;
  logerror *cur = local_list;
  unsigned int filtered_count = 0;

  while(cur != NULL) {
    next = cur->next;
    if ((*filter)(cur, data) == mode) { //Move to filtered
      local_list = errorlist_remove_error(local_list, cur);
      if (last_failed == NULL) {
        local_filtered = cur;
      } else {
        last_failed->next = cur;
        cur->prev = last_failed;
        cur->next = NULL;
      }
      last_failed = cur;
      ++filtered_count;
    }
    cur = next;
  }

  *filtered = local_filtered;
  *list = local_list;

  return filtered_count;
}
int filter_stringsearch(logerror *err, void *data) {
  if (data == NULL) return FILTER_PASS;
  char *needle = (char *)data;
  if (strstr(err->filename, needle) != NULL) {
    return FILTER_PASS;
  } 
  if (strstr(err->msg, needle) != NULL) {
    return FILTER_PASS; 
  }
  return FILTER_FAIL; 
}

//Ordering functions
void toggle_sort_direction(logfile *log) {
  log->sorting = -log->sorting;
  log->errors = errorlist_sort(log->errors, log->sorting);
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
  log->errors = errorlist_sort(log->errors, sort);
}

static inline void update_top_errortype(short *top, logerror *err) {
  if (errortype_worse(err->type, *top)) {
    *top = err->type;
  }
}

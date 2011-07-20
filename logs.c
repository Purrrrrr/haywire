#include "logs.h"
#include "parse_log.c"

#define SEEK_BUFFER_SIZE 1024

//Helper functions
void logfile_seek(FILE *logfile, int initial_row_count);
logerror *errorlist_insert(logerror *list, logerror *ins);
logerror *errorlist_insert_sorted(logerror *list, logerror *ins, int sorttype);

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
    fseek(logfile, 0, SEEK_SET);
    return;
  }
  fseek(logfile, 0, SEEK_END);
  if (initial_row_count == 0) return;

  int pos_in_buffer = 0;
  int row_count = 0;
  char *buffer;
  buffer = malloc(sizeof(char) * SEEK_BUFFER_SIZE);

  //Rewind a bit
  int read;
  long buffer_pos = ftell(logfile);
  if (buffer_pos == 0) return;
  long seek = buffer_pos % SEEK_BUFFER_SIZE;
  buffer_pos -= seek != 0 ? seek : SEEK_BUFFER_SIZE;
  fseek(logfile, buffer_pos, SEEK_SET);

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
      fseek(logfile, buffer_pos, SEEK_SET);
    }
  }
  
  //We goto here from the for loop that inspects the buffer.
  end:

  //If not enough rows, then the file is already seeked properly to the beginning
  if (row_count <= initial_row_count) {
    fseek(logfile, 0, SEEK_SET);
  }
  
  fseek(logfile, buffer_pos+pos_in_buffer+1, SEEK_SET);
}

int logfile_refresh(logfile *log) {
  int rowcount = 0;
  char *buffer = NULL;
  size_t n = 0;
  ssize_t len = 0;
  logerror *err;
  logerror *err_in_table;

  while(len = getline(&buffer, &n, log->file)) {
    if (len < 0) break;
    //We don't like 'em trailin newlines!
    if (len >= 1 && buffer[len-1] == '\n') buffer[len-1] = '\0';

    err = parse_error_line(buffer);
    if (err == NULL) continue;

    ++log->inspected_lines;
    
    //Get a possible duplicate entry in the hashtable
    err_in_table = ght_get(log->errortypes, err->linelength, err->logline);
    if (err_in_table == NULL) {
      ght_insert(log->errortypes, err, err->linelength, err->logline);
      log->errorlist = errorlist_insert(log->errorlist, err);
    } else {
      logerror_merge(err_in_table, err);
    }
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

logerror *errorlist_insert(logerror *list, logerror *ins) {
  ins->next = list;
  return ins;
}
logerror *errorlist_insert_sorted(logerror *list, logerror *ins, int sorttype) {
  if (list == NULL || errorlog_cmp(list, ins, sorttype) > 0) {
    ins->next = list;
    return ins;
  }
  
  logerror *cur = list;
  while(cur->next != NULL && errorlog_cmp(cur->next, ins, sorttype) <= 0) {
    cur = cur->next;
  }
  ins->next = cur->next;
  cur->next = ins;

  return list;
}
int errorlist_count(logfile *log) {
  return ght_size(log->errortypes);
}
logerror *errorlist_sort(logfile *log, int sorttype) {
  logerror *cur = log->errorlist;
  logerror *next;
  logerror *list = NULL;

  while(cur != NULL) {
    next = cur->next;
    list = errorlist_insert_sorted(list, cur, sorttype);
    cur = next;
  }

  log->errorlist = list;

  return list;
}

void logfile_remove_error(logfile *log, logerror *err) {
  ++log->empty_entries;
  err->count = 0;
}

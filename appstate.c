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

#include "appstate.h"
#include <unistd.h>
#include <string.h>
#include <time.h>

int parse_arguments(haywire_state *state, int argv, char *args[]) {
  if (argv < 2) {
    return 0;
  }

  state->relative_path = getcwd(NULL,0);

  //Initial log reading
  logfile *log = logfile_create();
  if (log == NULL) exit(EXIT_FAILURE);

  int read_lines = 10;
  int read_additional_filenames = 0;
  char *filename = NULL;

  for(int a = 1; a < argv; ++a) {
    char *arg = args[a];
    if (arg[0] == '-') {
      switch(arg[1]) {
        case 'n':
          ++a;
          if (!sscanf(args[a], "%d", &read_lines)) {
            logfile_close(log);
            return 0;
          }
          break;
        case 'w':
          read_lines = READ_ALL_LINES;
          break;
        case 'a':
          if (filename == NULL) {
            logfile_close(log);
            return 0;
          }
          read_additional_filenames = 1;
          break;
      }
    } else if (filename == NULL) {
      filename = arg;
    } else if (read_additional_filenames) {
      if (!logfile_add_file(log, arg, READ_ALL_LINES)) {
        printf("Error opening file %s\n", arg);
        exit(EXIT_FAILURE);
      }
    } else {
      logfile_close(log);
      return 0;
    }
  }
  if (filename == NULL) {
    logfile_close(log);
    return 0;
  }
  if (!logfile_add_file(log, filename, read_lines)) {
    printf("Error opening file %s\n", filename);
    exit(EXIT_FAILURE);
  }

  state->log = log;
  
  return 1;
}

void print_usage() {
  printf("Usage: haywire [-w|-n lines] filename [-a filename [filename] ...]\n");
  printf("-n Determines how many last lines are included from the file.\n");
  printf("   The default is 10.\n");
  printf("-w Read the whole file\n");
  printf("-a Tells the analyzer to append an additional file to the analysis without following it\n");
  exit(EXIT_SUCCESS);
}
void exit_with_error(char *err) {
  fprintf(stderr, "%s\n", err);
  exit(EXIT_FAILURE);
}

//Bell functions
void toggle_bell_type(haywire_state *app) {
  ++app->bell_type;
  if (app->bell_type > BELLTYPE_MAX) app->bell_type = 0;
}
void toggle_bell_level(haywire_state *app) {
  switch(app->bell_level) {
    case E_PARSE:
    app->bell_level = E_NOTICE; break;
    case E_NOTICE:
    app->bell_level = E_MISSING_FILE; break;
    case E_MISSING_FILE:
    app->bell_level = E_PARSE; break;
  }
}

int get_selected(haywire_state *app) {
  logerror *err = app->log->errors;
  int n = 0;
  while(err != app->selected) {
    if (err == NULL) return -1;
    ++n;
    err = err->next;
  }
  return n;
}
void select_nth(haywire_state *app, int n) {
  logerror *err = app->log->errors;
  while(n > 0) {
    if (err == NULL) break;
    err = err->next;
    n--;
  }
  app->selected = err;
}

char *get_log_time(logerror_occurrence *err) {
  static char buff[7] = "";
  time_t now = time(NULL);
  struct tm date;
  struct tm now_date;
  localtime_r(&err->date, &date);
  localtime_r(&now, &now_date);
  
  char *format = "";
  if (now < err->date + 60*60*12) {
    format = " %H:%M";
  } else if (date.tm_year == now_date.tm_year 
          && date.tm_yday == now_date.tm_yday) {
    format = " %H:%M";
  } else {
    format = "%d %b";
  }
  strftime(buff, sizeof(buff), format, &date); 
  
  return buff;
}

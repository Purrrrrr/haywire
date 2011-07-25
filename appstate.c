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

void time_status_print();
void bell_status_print(haywire_state *app);
void order_status_print(haywire_state *app);

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

int status_x = 0;

void print_statusline(haywire_state *app, int maxrows) {
  status_x = getmaxx(app->screen);

  bell_status_print(app);
  print_to_status(" - ", 0); 
  order_status_print(app);
  print_to_status(" - ", 0); 
  time_status_print();

  int bottomitem = app->scroll + maxrows;
  int itemcount = errorlist_count(app->log);
  if (bottomitem > itemcount) bottomitem = itemcount;

  move(0,0);
  printw("%d-%d of %d", app->scroll+1, bottomitem, itemcount);
}
void print_to_status(char *msg, short color) {
  if (msg == NULL) return;
  status_x -= strlen(msg);
  if (color) attron(COLOR_PAIR(color));
  mvprintw(0,status_x, msg);
  if (color) attroff(COLOR_PAIR(color));
}

void time_status_print() {
  char timestr[40] = "";
  time_t t = time(NULL);
  struct tm now;
  localtime_r(&t, &now);
  strftime(timestr, sizeof(timestr), "%H:%M %d %b %Y", &now);
  print_to_status(timestr, 0); 
}

void order_status_print(haywire_state *app) {
  print_to_status(" ", 0); 
  int sort = app->log->sorting;
  if (sort < 0) {
    print_to_status(" REVERSED", 0);
    sort = -sort;
  }
  switch(sort) {
    case SORT_TYPE:
    print_to_status("ERRORTYPE", 0); break;
    case SORT_DATE:
    print_to_status("DATE", 0); break;
    case SORT_COUNT:
    print_to_status("COUNT", 0); break;

  }
  print_to_status("SORT: ", 0); 
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
void ring_bells(haywire_state *app) {
  logfile *log = app->log;
  if (app->bell_type == BELL_ON_NEW_ERROR && log->worst_new_line && log->worst_new_line <= app->bell_level) {
    beep();
  } else if (app->bell_type == BELL_ON_NEW_ERRORTYPE && log->worst_new_type && log->worst_new_type <= app->bell_level) {
    beep();
  }
  log->worst_new_line = 0;
  log->worst_new_type = 0;
}
void bell_status_print(haywire_state *app) {
  char *errtype = NULL;
  short color = 0;
  if (app->bell_type != NO_BELL) {
    switch(app->bell_level) {
      case E_PARSE:
      color = LOG_RED;
      errtype = "FATAL ERRORS";
      break;
      case E_NOTICE:
      color = LOG_YELLOW;
      errtype = "PHP ERRORS";
      break;
      case E_MISSING_FILE:
      color = LOG_GREEN;
      errtype = "ERRORS AND 404's";
      break;
    }
    print_to_status(errtype, color);
    if (app->bell_type == BELL_ON_NEW_ERRORTYPE) {
      print_to_status("NEW ", color);
    } else {
      print_to_status("ALL ", color);
    }
    print_to_status("BELL: ", 0);
  } else {
    print_to_status("BELL: NONE", 0);
  }
}

int get_selected(haywire_state *app) {
  logerror *err = app->log->errorlist;
  int n = 0;
  while(err != app->selected) {
    if (err == NULL) return -1;
    ++n;
    err = err->next;
  }
  return n;
}
void select_nth(haywire_state *app, int n) {
  logerror *err = app->log->errorlist;
  while(n > 0) {
    if (err == NULL) break;
    err = err->next;
    n--;
  }
  app->selected = err;
}

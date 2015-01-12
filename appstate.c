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
#include <sys/types.h>
#include <pwd.h>

static char *defaultLogFormat = "[%t] [%l] [client\\ %a] %M% ,\\ referer:\\ %{referer}i";

typedef struct haywire_parse_state {
  logfile *log;
  int read_lines;
  char *filename;
} haywire_parse_state;

char *get_default_config_file();
void parse_config_file(haywire_parse_state *state, char *filename);

/* Parses the given command line arguments and
 * the application configuration file
 */
int parse_arguments(haywire_state *state, int argv, char *args[]) {
  int filename_specified = 0;
  int read_additional_filenames = 0;
  haywire_parse_state p_state = {
    NULL,
    10,
    NULL,
  };

  state->relative_path = getcwd(NULL,0);
  p_state.log = logfile_create();
  p_state.log->logformatString  = defaultLogFormat;
  if (p_state.log == NULL) exit_with_error("Could not allocate data structure");
  
  //Get default config file path and parse that file
  char *configfile = get_default_config_file();
  parse_config_file(&p_state, configfile);
  free(configfile);

  for(int a = 1; a < argv; ++a) {
    char *arg = args[a];
    if (arg[0] == '-') {
      switch(arg[1]) {
        case 'n':
          ++a;
          if (a >= argv) return 0;
          if (!sscanf(args[a], "%d", &p_state.read_lines)) {
            logfile_close(p_state.log);
            return 0;
          }
          break;
        case 'w':
          p_state.read_lines = READ_ALL_LINES;
          break;
        case 'F':
          ++a;
          if (a >= argv) return 0;
          logParseToken *parser = parseLogFormatString(args[a]);
          if (parser != NULL) {
            p_state.log->logformatString = args[a];
            p_state.log->logformat = parser;
          } else {
            fprintf(stderr, "Error parsing log format \"%s\"\n", args[a]);
            logfile_close(p_state.log);
            return 0;
          }
          break;
        case 'a':
          if (p_state.filename == NULL) {
            logfile_close(p_state.log);
            return 0;
          }
          read_additional_filenames = 1;
          break;
      }
    } else if (p_state.filename == NULL || filename_specified == 0) {
      filename_specified = 1;
      p_state.filename = arg;
    } else if (read_additional_filenames) {
      if (!logfile_add_file(p_state.log, arg, READ_ALL_LINES)) {
        fprintf(stderr, "Error opening file %s\n", arg);
        exit(EXIT_FAILURE);
      }
    } else {
      logfile_close(p_state.log);
      return 0;
    }
  }

  if (p_state.filename == NULL) {
    logfile_close(p_state.log);
    return 0;
  }
  if (!logfile_add_file(p_state.log, p_state.filename, p_state.read_lines)) {
    fprintf(stderr, "Error opening file %s\n", p_state.filename);
    exit(EXIT_FAILURE);
  }

  state->log = p_state.log;

  //Filename was copied from config file
  if (!filename_specified) {
    free(p_state.filename);
  }
  
  return 1;
}

static char *config_filename = ".haywirerc";
static char *config_logformat_str = "log-format: ";
static char *config_main_filename_str = "follow-file: ";
static char *config_add_filename_str = "read-file: ";
static char *config_linecount_str = "linecount: ";
static char *config_linecount_all_str = "all";

/* Parses the given configuration file updating the given parse state
 * with the linecount and filename specified in the file.
 * Opens any files specified by read-file directives immediately
 */
void parse_config_file(haywire_parse_state *state, char *filename) {
  linereader *file = linereader_open(filename, READ_ALL_LINES);
  if (file == NULL) return;

  char *line = NULL;
  char *val = NULL;

  while((line = linereader_getline(file)) != NULL) {
    if (strstr(line, config_main_filename_str) == line) {
      val = line + strlen(config_main_filename_str);
      if (state->filename != NULL) {
        linereader_close(file);
        free(state->filename);
        return;
      }
      state->filename = calloc(strlen(val) + 1, 1);
      strcpy(state->filename, val);
      
      continue;
    }
    if (strstr(line, config_add_filename_str) == line) {
      val = line + strlen(config_add_filename_str);
      if (!logfile_add_file(state->log, val, READ_ALL_LINES)) {
        fprintf(stderr, "Error opening file %s\n", val);
        exit(EXIT_FAILURE);
      }
      continue;
    }
    if (strstr(line, config_logformat_str) == line) {
      val = line + strlen(config_logformat_str);
      logParseToken *parser = parseLogFormatString(val);
      if (parser != NULL) {
        state->log->logformatString  = val;
        state->log->logformat = parser;
      } else {
        fprintf(stderr, "Error parsing log format \"%s\"\n", val);
        exit(EXIT_FAILURE);
      }
      continue;
    }
    if (strstr(line, config_linecount_str) == line) {
      val = line + strlen(config_linecount_str);
      if (strstr(val, config_linecount_all_str) == val) {
        state->read_lines = READ_ALL_LINES;
      } else if (!sscanf(val, "%d", &state->read_lines)) {
        linereader_close(file);
        return;
      }
      continue;
    }
  }

  linereader_close(file);
}
/* Returns something like /home/user/.haywirerc in a malloc'd buffer */
char *get_default_config_file() {
  uid_t uid = getuid();
  struct passwd *uinfo = getpwuid(uid);

  int dirlen = strlen(uinfo->pw_dir);
  char *buffer = malloc(dirlen + 1 + strlen(config_filename) + 1);
  if (buffer == NULL) {
    exit_with_error("Could not allocate filename buffer.");
  }

  strcpy(buffer, uinfo->pw_dir);
  buffer[dirlen] = '/';
  strcpy(buffer + dirlen + 1, config_filename);
  buffer[dirlen + 1 + strlen(config_filename)] = '\0';
  
  return buffer; 
}

void print_usage() {
  printf("Usage: haywire [-F format] [-w|-n lines] filename [-a filename [filename] ...]\n");
  printf("-n Determines how many last lines are included from the file.\n");
  printf("   The default is 10.\n");
  printf("-w Read the whole file\n");
  printf("-F The format of the logs as an Apache ErrorLogFormat string\n");
  printf("-a Tells the analyzer to append an additional file(s) to the analysis without following it\\them\n");
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

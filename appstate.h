#ifndef APPSTATE_H
#define APPSTATE_H
#include <curses.h>
#include "logs.h"

#define LOG_RED 1
#define LOG_YELLOW 2
#define LOG_GREEN 3

typedef struct {
  int update_delay;
  char *relative_path;
  logfile *log;
  int sorting;
  logerror *selected;
  int scroll;
  WINDOW *screen;
} haywire_state;

//const haywire_state default_state;
static const haywire_state default_state = {10, NULL, NULL, SORT_DEFAULT, NULL, 0, NULL};

int parse_arguments(haywire_state *state, int argv, char *args[]);
void print_usage();

#endif

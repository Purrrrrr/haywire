#ifndef APPSTATE_H
#define APPSTATE_H
#include <curses.h>
#include "logs.h"

#define LOG_RED 1
#define LOG_YELLOW 2
#define LOG_GREEN 3

#define NO_BELL 0
#define BELL_ON_NEW_ERROR 1
#define BELL_ON_NEW_ERRORTYPE 2
#define BELLTYPE_MAX 2

typedef struct {
  int bell_level; //One of the log error levels
  short bell_type;
  time_t last_error_date;
  int update_delay;
  char *relative_path;
  logfile *log;
  short sorting;
  logerror *selected;
  int scroll;
  short show_info;
  WINDOW *screen;
} haywire_state;

//const haywire_state default_state;
static const haywire_state default_state = {E_PARSE, BELL_ON_NEW_ERROR, 0, 10, NULL, NULL, SORT_DEFAULT, NULL, 0, 1, NULL};

int parse_arguments(haywire_state *state, int argv, char *args[]);
void print_usage();

void print_statusline(haywire_state *app, int maxrows);
void print_to_status(char *msg, short color);

void toggle_sort_type(haywire_state *app);
void toggle_sort_direction(haywire_state *app);

void toggle_bell_type(haywire_state *app);
void toggle_bell_level(haywire_state *app);
void ring_bells(haywire_state *app);

#endif

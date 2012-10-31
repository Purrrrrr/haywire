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

#ifndef APPSTATE_H
#define APPSTATE_H
#include <curses.h>
#include "loglist.h"

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
  logerror *selected;
  int scroll;
  short show_info;
  WINDOW *screen;
} haywire_state;

//const haywire_state default_state;
static const haywire_state default_state = {
    E_PARSE,
    BELL_ON_NEW_ERROR,
    0, //Last error date
    10, //Update delay
    NULL, //Relative path
    NULL, //Log
    NULL, //Selected entry
    0, //Scroll
    1, //Show_info
    NULL //Screen
};

int parse_arguments(haywire_state *state, int argv, char *args[]);
void print_usage();

void print_statusline(haywire_state *app, int maxrows);
void print_to_status(char *msg, short color);

void toggle_bell_type(haywire_state *app);
void toggle_bell_level(haywire_state *app);
void ring_bells(haywire_state *app);

int get_selected(haywire_state *app);
void select_nth(haywire_state *app, int n);

//Returns a string representation of the log occurrence time
char *get_log_time(logerror_occurrence *err);

#endif

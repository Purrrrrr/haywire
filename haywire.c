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

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "appstate.h"
#include "screen.h"
#include "loglist.h"

#define STATUS_LINE_COUNT 2
#define MAX_LINE_LEN 256

#define MODE_NORMAL 0
#define MODE_SEARCH 1

haywire_state app;
short mode = MODE_NORMAL; //Mode of input
int infobox_size = 0;

//Helper buffers
size_t filename_len = 256;
char *filename = NULL; 

size_t search_filter_buffer_len = 256;
char *search_filter = NULL; 
unsigned int search_filter_len = 0;
unsigned int search_filter_pos = 0;

short process_keyboard(int c);
short process_search_keyboard(int c);
void init_screen();
int list_row_count();
void list_select_change(int direction);
void list_scroll_page(int scroll);
void list_scroll(int scroll);
int str_linecount(char *str, int linelen, int *x);
void display_infobox();
void display_logs();
void print_error(logerror *err, int selected, int row);
void vim(logerror *err);

int main(int argv, char *args[]) {
  app = default_state;
  if (!parse_arguments(&app, argv, args)) print_usage();

  filename = malloc(filename_len);
  if (filename == NULL) exit_with_error("Error allocating memory");

  search_filter = calloc(search_filter_buffer_len, sizeof(char));
  if (search_filter == NULL) exit_with_error("Error allocating memory");
  
  //Init display
  init_screen();

/*  logfile_refresh(app.log);
  errorlist_sort(app.log, app.log->sorting);
  select_nth(&app, 0); */

  short redraw_treshold = 600/app.update_delay;
  short last_updated_in = redraw_treshold; //Update immediately to show the screen.

  while(1) {
    int c = getch();
    short modified;
      
    if (mode == MODE_NORMAL && c == 'q') break;
    logfile_refresh(app.log);

    switch(mode) {
      case MODE_NORMAL:
        modified = process_keyboard(c);
      break;
      case MODE_SEARCH:
        modified = process_search_keyboard(c);
        break;
    }

    if (app.log->worstNewLine || app.log->worstNewType) modified = 1;
    if (last_updated_in >= redraw_treshold) modified = 1;
    
    if (modified) {
      clear();
      display_infobox();
      display_logs();
      ring_bells(&app);
      last_updated_in = 0;
    } else last_updated_in++;
  }

  endwin();
  logfile_close(app.log);

}

/* Process the given key and return 1 if the screen needs to be refreshed */
short process_keyboard(int c) {
  short modified = 1; //By default every keystroke updates the screen

  switch(c) {
    case '/': 
      mode = MODE_SEARCH;
      break; 
    case 'c': 
      logfile_clear(app.log);
      app.selected = NULL;
      app.scroll = 0;
      break;
    case 'w': 
      logfile_set_filter(app.log, &filter_filename, "libra");
      break;
    case 'W': 
      logfile_set_filter(app.log, NULL, NULL);
      break;
    case 'j': 
    case KEY_DOWN: 
      app.show_info ? list_select_change(1) : list_scroll(1);
      break;
    case 'k': 
    case KEY_UP: 
      app.show_info ? list_select_change(-1) : list_scroll(-1);
      break;
    case ' ':
    case KEY_NPAGE: 
      list_scroll_page(1);
      break;
    case KEY_PPAGE: 
      list_scroll_page(-1);
      break;
    case 'B':
      toggle_bell_type(&app);
      break;
    case 'b':
      toggle_bell_level(&app);
      break;
    case 'o':
      toggle_sort_type(app.log);
      break;
    case 'O':
      toggle_sort_direction(app.log);
      break;
    case 'i':
    case 'f':
      app.show_info = !app.show_info;
      if (app.show_info) {
        logerror *err = app.log->errors;
        int i = app.scroll;
        while(i > 0 && err != NULL) {
          i--;
          err = err->next;
        }
        app.selected = err;
      } else {
        app.selected = NULL;
      }
      break;
    case 'v':
    case '\r':
    case '\n':
    case KEY_ENTER:
      vim(app.selected);
      break;
    case KEY_RESIZE: //Prevent the default from setting modified = 0;
      break;
    default:
      modified = 0; //No keystroke, not modified
  }

  return modified;
}
short process_search_keyboard(int c) {
  switch(c) {
    case '\r':
    case '\n':
    case KEY_ENTER:
      mode = MODE_NORMAL;
      break;
    case KEY_BACKSPACE:
      if (search_filter_pos == 0) break;

      --search_filter_len;
      --search_filter_pos;
      search_filter[search_filter_pos] = '\0';

      break;
    case ERR: //halfdelay returns nothing
      break;
    default:
      if (search_filter_len == search_filter_buffer_len) {
        search_filter_len *= 2;
        search_filter = realloc(search_filter, search_filter_len);

        if (search_filter == NULL) {
          exit_with_error("Error allocating memory");
        }
      }
      search_filter[search_filter_pos] = (char)c;
      search_filter_pos++;
      search_filter_len++;
      break;
  }
  
  if (search_filter_len > 0) {
    logfile_set_filter(app.log, &filter_filename, search_filter);
  } else {
    logfile_set_filter(app.log, NULL, NULL);
  }

  return 1;
}

int list_row_count() {
  int x = 0;
  int maxrows = 0;
  getmaxyx(app.screen, maxrows, x);
  if (app.show_info) maxrows -= infobox_size;
  return maxrows - STATUS_LINE_COUNT;
}
void list_select_change(int direction) {
  logerror *err = app.log->errors;
  int rows = list_row_count();

  if (app.selected == NULL) {
    app.selected = err;
    return;
  }

  int n = get_selected(&app);
  if (direction > 0) {
    if (app.selected->next != NULL) {
      ++n;
      app.selected = app.selected->next;
    }
  } else {
    if (n == 0) goto end;
    --n;
    select_nth(&app, n);
  }
  end:
  if (n+1 >= app.scroll + rows) list_scroll(n-app.scroll-rows+1);
  if (n <= app.scroll) list_scroll(n-app.scroll);
}
void list_scroll_page(int scroll) {
  list_scroll(scroll*list_row_count());
}
void list_scroll(int scroll) {
  app.scroll += scroll;
  int maxscroll = errorlist_count(app.log);
  int rows = list_row_count();
  maxscroll -= rows;
  if (app.scroll > maxscroll) app.scroll = maxscroll;
  if (app.scroll < 0) app.scroll = 0;
  if (app.selected != NULL) {
    int n = get_selected(&app);
    if (n >= app.scroll + rows) select_nth(&app, app.scroll+rows-1);
    if (n < app.scroll) select_nth(&app, app.scroll);
  }
}

static const char location_text[] = "Location:";
static const char referer_text[] = "Referer:";
static const char stack_trace_text[] = "Stack Trace:";

void display_infobox() {
  infobox_size = 0;
  if (!app.show_info || app.selected == NULL) return;
  
  int cols,rows;
  cols=rows=0;
  getmaxyx(app.screen, rows, cols);

  int x = 0;
  size_t msg_lines = str_linecount(app.selected->msg, cols, &x);
  x = sizeof(location_text);
  size_t filename_lines = str_linecount(app.selected->filename, cols, &x);

  size_t linenr_length = 1;
  int i = app.selected->linenr;
  while(i > 10) { i /= 10; ++linenr_length; }
  if (x + linenr_length + 1 > cols) ++filename_lines;

  size_t referer_lines = 0;
  if (app.selected->latest_occurrence->referer != NULL) {
    x = 0; //sizeof(referer_text);
    referer_lines = str_linecount(app.selected->latest_occurrence->referer, cols, &x) + 1;
  }
  size_t stack_trace_lines = 0;
  if (app.selected->latest_occurrence->stack_trace != NULL) {
    x = 0; //sizeof(stack_trace_text);
    stack_trace_lines = str_linecount(app.selected->latest_occurrence->stack_trace, cols, &x) + 1;
  }

  infobox_size = 1+msg_lines+filename_lines+referer_lines+stack_trace_lines;
  
  int y = rows-infobox_size;
  move(y, 0);
  standout();
  hline(' ', cols);
  mvprintw(y, 0, " Selected message:");
  mvprintw(y, cols-23, " (Toggle info using i)");
  standend();

  mvprintw(++y, 0, "%s", app.selected->msg);
  y += msg_lines;

  if (referer_lines) {
    attron(A_BOLD);
    mvprintw(y, 0, "%s ", referer_text);
    attroff(A_BOLD);
    mvprintw(y+1, 0, "%s", app.selected->latest_occurrence->referer);
    y += referer_lines;
  }

  if (stack_trace_lines) {
    attron(A_BOLD);
    mvprintw(y, 0, "%s ", stack_trace_text);
    attroff(A_BOLD);
    mvprintw(y+1, 0, "%s", app.selected->latest_occurrence->stack_trace);
    y += stack_trace_lines;
  }

  attron(A_BOLD);
  mvprintw(y, 0, "%s ", location_text);
  attroff(A_BOLD);
  printw("%s:%d", app.selected->filename, app.selected->linenr);
  //printw("%d", infobox_size);

}
int str_linecount(char *str, int linelen, int *x) {
  int lines = 1;
  while(*str != '\0') {
    if (*str == '\n') {
      ++lines;
      *x = 0;
    } else {
      ++(*x);
      if (*x >= linelen) {
        ++lines;
        *x = 0;
      }
    }
    ++str;
  }
  return lines;
}

void display_logs() {

  int maxrows = list_row_count();
  print_statusline(&app, maxrows);
  
  move(1, 0);
  standout();
  hline(' ', getmaxx(app.screen));
  printw("  TIME CNT MSG");
  standend();

  int skip = app.scroll;
  int i = 0;
  logerror *err = app.log->errors;
  while(err != NULL) {
    if (skip > 0) {
      --skip;
    } else if (i < maxrows) {
      print_error(err, app.show_info && err == app.selected, i+2);
      ++i;
    } else {
      break;
    }
    err = err->next;
  } 

  refresh();
}
void print_error(logerror *err, int selected, int row) {
  logerror_nicepath(err, app.relative_path, &filename, &filename_len);
  short color = 0;
  char linebuffer[MAX_LINE_LEN] = "";

  switch(err->type) {
    case E_ERROR:
    case E_PARSE:
      color = LOG_RED;
      break;
    case E_WARNING:
    case E_NOTICE:
      color = LOG_YELLOW;
      break;
    default:
      color = LOG_GREEN;
  }
  attron(COLOR_PAIR(color));
  if (selected) standout();

  snprintf(linebuffer, sizeof(linebuffer), "%s %3d %s: %s:%d %*s", get_log_time(err->latest_occurrence), err->count, err->msg, filename, err->linenr, (int)(sizeof(linebuffer)), "");
  move(row, 0);
  addnstr(linebuffer, getmaxx(app.screen));

  if (selected) standend();
  attroff(COLOR_PAIR(color));
}
void vim(logerror *err) {
  if (err == NULL) return;
  endwin();
  
  pid_t id = fork();
  if (id == 0) {
    if (err->linenr >= 0) {
      char linenr[64] = "";
      snprintf(linenr, sizeof(linenr), "+%d", err->linenr);
      execlp("vim", "vim", linenr, err->filename ,NULL );
    } else {
      execlp("vim", "vim", err->filename ,NULL );
    }
    exit(EXIT_SUCCESS);
  } else if (id != -1) {
    int status = 0;
    wait(&status);
  }

  init_screen();
}

void init_screen() {
  app.screen = initscr();
  noecho();
  nonl();
  halfdelay(app.update_delay);
  keypad(app.screen, TRUE);
  clear();
  curs_set(0);

  if(has_colors() == FALSE) {
    endwin();
    exit_with_error("Your terminal does not support color");
  }
  start_color();
  use_default_colors();
  init_pair(LOG_RED,COLOR_RED,-1);
  init_pair(LOG_YELLOW,COLOR_YELLOW,-1);
  init_pair(LOG_GREEN,COLOR_GREEN,-1);

  refresh();
}


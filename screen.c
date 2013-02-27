#include "screen.h"
#include <unistd.h>
#include <string.h>
#include <time.h>

#define STATUS_LINE_COUNT 2
#define MAX_LINE_LEN 256
static size_t filename_len = 0;
static char *filename = NULL; 

void display_infobox(haywire_state *app);
void display_logs(haywire_state *app, int maxrows);
void display_statusline(haywire_state *app, int maxrows);
void print_to_status(char *msg, short color);
int str_linecount(char *str, int linelen, int *x);
void time_status_print();
void bell_status_print(haywire_state *app);
void order_status_print(haywire_state *app);

/****************************
 *   Utility functions      *
 ****************************/
void init_screen(haywire_state *app) {
  app->screen.screen = initscr();
  noecho();
  nonl();
  halfdelay(app->update_delay);
  keypad(app->screen.screen, TRUE);
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
int list_row_count(haywire_state *app) {
  int x = 0;
  int maxrows = 0;
  getmaxyx(app->screen.screen, maxrows, x);
  if (app->screen.show_info) maxrows -= app->screen.infobox_size;
  return maxrows - STATUS_LINE_COUNT;
}


/****************************
 * Main rendering functions *
 ****************************/

static const char location_text[] = "Location:";
static const char referer_text[] = "Referer:";
static const char stack_trace_text[] = "Stack Trace:";

void refresh_screen(haywire_state *app) {
  int maxrows = list_row_count(app);
  display_infobox(app);
  display_logs(app, maxrows);
  display_statusline(app, maxrows);
}

void display_logs(haywire_state *app, int maxrows) {
  move(1, 0);
  standout();
  hline(' ', getmaxx(app->screen.screen));
  printw("  TIME CNT MSG");
  standend();

  int skip = app->scroll;
  int i = 0;
  logerror *err = app->log->errors;
  while(err != NULL) {
    if (skip > 0) {
      --skip;
    } else if (i < maxrows) {
      print_error(app, err, app->screen.show_info && err == app->selected, i+2);
      ++i;
    } else {
      break;
    }
    err = err->next;
  } 

  refresh();
}
/* Displays the infobox and updates into the app state the size of it in rows */
void display_infobox(haywire_state *app) {
  int infobox_size = 0;
  if (!app->screen.show_info || app->selected == NULL) return;
  
  int cols,rows;
  cols=rows=0;
  getmaxyx(app->screen.screen, rows, cols);

  int x = 0;
  size_t msg_lines = str_linecount(app->selected->msg, cols, &x);
  x = sizeof(location_text);
  size_t filename_lines = str_linecount(app->selected->filename, cols, &x);

  size_t linenr_length = 1;
  int i = app->selected->linenr;
  while(i > 10) { i /= 10; ++linenr_length; }
  if (x + linenr_length + 1 > cols) ++filename_lines;

  size_t referer_lines = 0;
  if (app->selected->latest_occurrence->referer != NULL) {
    x = 0; //sizeof(referer_text);
    referer_lines = str_linecount(app->selected->latest_occurrence->referer, cols, &x) + 1;
  }
  size_t stack_trace_lines = 0;
  if (app->selected->latest_occurrence->stack_trace != NULL) {
    x = 0; //sizeof(stack_trace_text);
    stack_trace_lines = str_linecount(app->selected->latest_occurrence->stack_trace, cols, &x) + 1;
  }

  infobox_size = 1+msg_lines+filename_lines+referer_lines+stack_trace_lines;
  
  int y = rows-infobox_size;
  move(y, 0);
  standout();
  hline(' ', cols);
  mvprintw(y, 0, " Selected message:");
  mvprintw(y, cols-23, " (Toggle info using i)");
  standend();

  mvprintw(++y, 0, "%s", app->selected->msg);
  y += msg_lines;

  if (referer_lines) {
    attron(A_BOLD);
    mvprintw(y, 0, "%s ", referer_text);
    attroff(A_BOLD);
    mvprintw(y+1, 0, "%s", app->selected->latest_occurrence->referer);
    y += referer_lines;
  }

  if (stack_trace_lines) {
    attron(A_BOLD);
    mvprintw(y, 0, "%s ", stack_trace_text);
    attroff(A_BOLD);
    mvprintw(y+1, 0, "%s", app->selected->latest_occurrence->stack_trace);
    y += stack_trace_lines;
  }

  attron(A_BOLD);
  mvprintw(y, 0, "%s ", location_text);
  attroff(A_BOLD);
  printw("%s:%d", app->selected->filename, app->selected->linenr);
  //printw("%d", infobox_size);
  
  app->screen.infobox_size = infobox_size;
}

/****************************
 *   Status line drawing    *
 ****************************/
int status_x = 0;

void display_statusline(haywire_state *app, int maxrows) {
  status_x = getmaxx(app->screen.screen);

  bell_status_print(app);
  print_to_status(" - ", 0); 
  order_status_print(app);
  print_to_status(" - ", 0); 
  time_status_print();

  int bottomitem = app->scroll + maxrows;
  int itemcount = errorlist_count(app->log);
  if (bottomitem > itemcount) bottomitem = itemcount;

  int scroll = app->scroll + 1;
  if (itemcount == 0) scroll = 0;

  move(0,0);
  printw("%d-%d of %d", scroll, bottomitem, itemcount);

  if (app->screen.status_line_printer != NULL) {
    (*app->screen.status_line_printer)(app, getcury(app->screen.screen), getcurx(app->screen.screen));
  }
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

/* Ring the terminal bell according to app settings */
void ring_bells(haywire_state *app) {
  logfile *log = app->log;
  if (app->bell_type == BELL_ON_NEW_ERROR && log->worstNewLine && log->worstNewLine <= app->bell_level) {
    beep();
  } else if (app->bell_type == BELL_ON_NEW_ERRORTYPE && log->worstNewType && log->worstNewType <= app->bell_level) {
    beep();
  }
  log->worstNewLine = 0;
  log->worstNewType = 0;
}

/****************************
 *    Log file printing     *
 ****************************/

void print_error(haywire_state *app, logerror *err, int selected, int row) {
  logerror_nicepath(err, app->relative_path, &filename, &filename_len);
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
  addnstr(linebuffer, getmaxx(app->screen.screen));

  if (selected) standend();
  attroff(COLOR_PAIR(color));
}


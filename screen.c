#include "screen.h"
#include <unistd.h>
#include <string.h>
#include <time.h>

void time_status_print();
void bell_status_print(haywire_state *app);
void order_status_print(haywire_state *app);

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
  if (app->log->filter == &filter_filename) {
    attron(A_BOLD);
    printw(" Filter: ");
    attroff(A_BOLD);
    printw("%s", (char *)app->log->filter_data);
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "appstate.h"
#include "logs.h"

#define STATUS_LINE_COUNT 2
#define MAX_LINE_LEN 256
haywire_state app;
int infobox_size = 0;

//Helper buffers
size_t filename_len = 256;
char *filename = NULL; 

void init_screen();
int list_row_count();
void list_select_change(int direction);
void list_scroll_page(int scroll);
void list_scroll(int scroll);
void display_infobox();
void display_logs();
void print_error(logerror *err, int selected, int row);
char *get_log_time(logerror *err);
void vim();

int main(int argv, char *args[]) {
  app = default_state;
  if (!parse_arguments(&app, argv, args)) print_usage();

  filename = malloc(filename_len);
  if (filename == NULL) exit(EXIT_FAILURE);
  
  //Init display
  init_screen();

  while(1) {
    int c = getch();
    if (c == 'q') break;
    
    logfile_refresh(app.log);
    errorlist_sort(app.log, app.sorting);

    switch(c) {
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
      toggle_sort_type(&app);
      errorlist_sort(app.log, app.sorting);
      break;
      case 'O':
      toggle_sort_direction(&app);
      errorlist_sort(app.log, app.sorting);
      break;
      case 'i':
      case 'f':
      app.show_info = !app.show_info;
      break;
      case 'v':
      vim();
      break;
    }
    clear();
    display_infobox();
    display_logs();
    ring_bells(&app);
  }

  endwin();
  logfile_close(app.log);

}
int list_row_count() {
  int x = 0;
  int maxrows = 0;
  getmaxyx(app.screen, maxrows, x);
  if (app.show_info) maxrows -= infobox_size;
  return maxrows - STATUS_LINE_COUNT;
}
void list_select_change(int direction) {
  logerror *err = app.log->errorlist;
  int rows = list_row_count();
  int n = 1;

  if (app.selected == NULL) {
    app.selected = err;
    return;
  }

  if (direction > 0) {
    while(err != NULL) {
      if (err == app.selected) {
        if (err->next == NULL) return;
        app.selected = err->next;
        goto end;
      }
      ++n;
      err = err->next;
    }
  } else {
    if (err == app.selected) return;
    n = 0;
    while(err != NULL) {
      if (err->next == app.selected) {
        app.selected = err;
        goto end;
      }
      ++n;
      err = err->next;
    }
    app.selected = app.log->errorlist;

    end:
    if (n >= app.scroll + rows) list_scroll(n-app.scroll-rows+1);
    if (n <= app.scroll) list_scroll(n-app.scroll);
  }
}
void list_scroll_page(int scroll) {
  list_scroll(scroll*list_row_count());
}
void list_scroll(int scroll) {
  app.scroll += scroll;
  int maxscroll = errorlist_count(app.log);
  maxscroll -= list_row_count();
  if (app.scroll > maxscroll) app.scroll = maxscroll;
  if (app.scroll < 0) app.scroll = 0;
}

void display_infobox() {
  infobox_size = 0;
  if (!app.show_info || app.selected == NULL) return;
  
  int cols,rows;
  cols=rows=0;
  getmaxyx(app.screen, rows, cols);
  
  size_t msg_length = strlen(app.selected->msg);
  size_t filename_length = strlen(app.selected->filename);
  size_t linenr_length = 1;
  int i = app.selected->linenr;
  while(i > 10) { i /= 10; ++linenr_length; }

  infobox_size = 1+(msg_length/cols + 1)+((filename_length+linenr_length+1)/cols + 1);
  
  int y = rows-infobox_size;
  move(y, 0);
  standout();
  hline(' ', cols);
  mvprintw(y, 0, " Selected message:");
  standend();

  mvprintw(++y, 0, "%s", app.selected->msg);
  y += (msg_length/cols + 1);
  mvprintw(y, 0, "%s:%d", app.selected->filename, app.selected->linenr);
  printw("%d", infobox_size);

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
  logerror *err = app.log->errorlist;
  while(err != NULL) {
    if (skip > 0) {
      --skip;
    } else if (i < maxrows) {
      print_error(err, app.show_info && err == app.selected, i+2);
      ++i;
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

  snprintf(linebuffer, sizeof(linebuffer), "%s %3d %s: %s:%d", get_log_time(err), err->count, err->msg, filename, err->linenr);
  move(row, 0);
  addnstr(linebuffer, getmaxx(app.screen));

  if (selected) standend();
  attroff(COLOR_PAIR(color));
}
char *get_log_time(logerror *err) {
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
void vim() {
  endwin();
  system("vim");
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
    printf("Your terminal does not support color\n");
    exit(EXIT_FAILURE);
  }
  start_color();
  use_default_colors();
  init_pair(LOG_RED,COLOR_RED,-1);
  init_pair(LOG_YELLOW,COLOR_YELLOW,-1);
  init_pair(LOG_GREEN,COLOR_GREEN,-1);

  refresh();
}


#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "appstate.h"
#include "logs.h"

haywire_state app;

//Helper buffers
size_t filename_len = 256;
char *filename = NULL; 

void init_screen();
void display_logs();
void print_error(logerror *err, int selected, int row);
void vim();

int main(int argv, char *args[]) {
  app = default_state;
  if (!parse_arguments(&app, argv, args)) print_usage();

  filename = malloc(filename_len);
  if (filename == NULL) exit(EXIT_FAILURE);
  
  //Init display
  init_screen();

  while(1) {
//    mvaddchstr(0,0,"Test");
    int c = getch();
    if (c == 'q') break;
    
    logfile_refresh(app.log);

    if (c == 'j' && app.scroll+1 < errorlist_count(app.log)) {
      ++app.scroll;
    }
    if (c == 'k' && app.scroll > 0) {
      --app.scroll;
    }
    if (c == 'v') vim();
    display_logs();
  }

  endwin();
  logfile_close(app.log);

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
  clear();

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
void display_logs() {
  int x = 0;
  int y = 0;
  getmaxyx(app.screen, y,x);

  clear();
  /*
  move(1,0);
  printw("%d,%d", y, x);
  hline(ACS_HLINE, 512);
  */
  printw("Scroll: %d (%d entries)", app.scroll, errorlist_count(app.log));
  
  int skip = app.scroll;
  int i = 0;
  logerror *err = errorlist_sort(app.log, app.sorting);
  while(err != NULL) {
    if (skip > 0) {
      --skip;
    } else {
      print_error(err, 0, i+1);
      ++i;
    }
    err = err->next;
  } 

  refresh();
}
void print_error(logerror *err, int selected, int row) {
  logerror_nicepath(err, app.relative_path, &filename, &filename_len);
  short color = 0;

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

  move(row, 0);
  printw("%3dx %s: %s:%d\n", err->count, err->msg, filename, err->linenr);

  if (selected) standend();
  attroff(COLOR_PAIR(color));


}

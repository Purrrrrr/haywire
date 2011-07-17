#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "appstate.h"
#include "logs.h"

#define STATUS_LINE_COUNT 1
haywire_state app;

//Helper buffers
size_t filename_len = 256;
char *filename = NULL; 

void init_screen();
int list_row_count();
void toggle_bell_type();
void toggle_bell_level();
void list_scroll_page(int scroll);
void list_scroll(int scroll);
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
    switch(c) {
      case 'j': 
      case KEY_DOWN: 
      list_scroll(1);
      break;
      case 'k': 
      case KEY_UP: 
      list_scroll(-1);
      break;
      case ' ':
      case KEY_NPAGE: 
      list_scroll_page(1);
      break;
      case KEY_PPAGE: 
      list_scroll_page(-1);
      break;
      case 'B':
      toggle_bell_type();
      break;
      case 'b':
      toggle_bell_level();
      break;
      case 'v':
      vim();
      break;
    }
    
    logfile_refresh(app.log);
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
  keypad(app.screen, TRUE);
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

int list_row_count() {
  int x = 0;
  int maxrows = 0;
  getmaxyx(app.screen, maxrows, x);
  return maxrows - STATUS_LINE_COUNT;
}
void toggle_bell_type() {
  ++app.bell_type;
  if (app.bell_type > BELLTYPE_MAX) app.bell_type = 0;
}
void toggle_bell_level() {
  switch(app.bell_level) {
    case E_PARSE:
    app.bell_level = E_NOTICE; break;
    case E_NOTICE:
    app.bell_level = E_MISSING_FILE; break;
    case E_MISSING_FILE:
    app.bell_level = E_PARSE; break;
  }
}
void list_scroll_page(int scroll) {
  list_scroll(scroll*list_row_count());
}
void list_scroll(int scroll) {
  app.scroll += scroll;
  if (app.scroll < 0) app.scroll = 0;
  int maxscroll = errorlist_count(app.log);
  maxscroll -= list_row_count();
  if (app.scroll > maxscroll) app.scroll = maxscroll;
}

void display_logs() {
  clear();
  move(0,0);

  char *bell_type = ""; 
  switch(app.bell_type) {
    case NO_BELL:
    bell_type = "- No bell on new errors"; break;
    case BELL_ON_NEW_ERROR:
    bell_type = "- Bell on "; break;
    case BELL_ON_NEW_ERRORTYPE:
    bell_type = "- Bell on new types of "; break;
  }
  char *bell_level = ""; 
  if (app.bell_type != NO_BELL) {
    switch(app.bell_level) {
      case E_PARSE:
      bell_level = "fatal errors"; break;
      case E_NOTICE:
      bell_level = "PHP errors"; break;
      case E_MISSING_FILE:
      bell_level = "all errors"; break;
    }
  }

  int maxrows = list_row_count();
  printw("%d-%d of %d entries %s%s", app.scroll+1, app.scroll+maxrows, errorlist_count(app.log), bell_type, bell_level);
  
  short bell_ringed = 0;
  int skip = app.scroll;
  int i = 0;
  logerror *err = errorlist_sort(app.log, app.sorting);
  while(err != NULL) {
    if (err->type <= app.bell_level) {
      if (app.bell_type == BELL_ON_NEW_ERROR && err->date > app.last_error_date) {
        bell_ringed = 1;
      } else if (app.bell_type == BELL_ON_NEW_ERRORTYPE && err->is_new) {
        bell_ringed = 1;
      }
    }
    err->is_new = 0;
    if (err->date > app.last_error_date) app.last_error_date = err->date;

    if (skip > 0) {
      --skip;
    } else {
      print_error(err, 0, i+1);
      ++i;
    }
    err = err->next;
  } 
  if (bell_ringed) {
    beep();
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

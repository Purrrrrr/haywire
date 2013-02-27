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

#define MODE_NORMAL 0
#define MODE_SEARCH 1

haywire_state app;
short mode = MODE_NORMAL; //Mode of input

//Helper buffers
size_t filename_len = 256;
char *filename = NULL; 

size_t search_filter_buffer_len = 256;
char *search_filter = NULL; 
unsigned int search_filter_len = 0;
unsigned int search_filter_pos = 0;

void change_mode(int mode);
short process_keyboard(int c);
short process_search_keyboard(int c);
void print_search_status_line(haywire_state *app, int y, int x);
void list_remove_selected();
void list_select_change(int direction);
void list_scroll_page(int scroll);
void list_scroll(int scroll);
void vim(logerror *err);

int main(int argv, char *args[]) {
  app = default_state;
  if (!parse_arguments(&app, argv, args)) print_usage();

  filename = malloc(filename_len);
  if (filename == NULL) exit_with_error("Error allocating memory");

  search_filter = calloc(search_filter_buffer_len, sizeof(char));
  if (search_filter == NULL) exit_with_error("Error allocating memory");
  
  //Init display
  init_screen(&app);

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
      refresh_screen(&app);
      ring_bells(&app);
      last_updated_in = 0;
    } else last_updated_in++;
  }

  endwin();
  logfile_close(app.log);

}

void change_mode(int newmode) {
  mode = newmode;
  switch(mode) {
    case MODE_NORMAL:
      curs_set(0);
      if (search_filter_len == 0) {
        app.screen.status_line_printer = NULL;
      }
      break;
    case MODE_SEARCH:
      curs_set(1);
    break;
  }
}
/* Process the given key and return 1 if the screen needs to be refreshed */
short process_keyboard(int c) {
  short modified = 1; //By default every keystroke updates the screen

  switch(c) {
    case '/': 
      change_mode(MODE_SEARCH);
      app.screen.status_line_printer = &print_search_status_line;
      break; 
    case 'd': 
      list_remove_selected();
      break;
    case 'c': 
      logfile_clear(app.log);
      app.selected = NULL;
      app.scroll = 0;
      break;
    case 'j': 
    case KEY_DOWN: 
      app.screen.show_info ? list_select_change(1) : list_scroll(1);
      break;
    case 'k': 
    case KEY_UP: 
      app.screen.show_info ? list_select_change(-1) : list_scroll(-1);
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
      app.screen.show_info = !app.screen.show_info;
      if (app.screen.show_info) {
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
      change_mode(MODE_NORMAL);
      break;
    case 21: //^U, clear line
      while(search_filter_len > 0) {
        search_filter[--search_filter_len] = '\0';
      }
      search_filter_pos = 0;
      change_mode(MODE_NORMAL);
      break; 
    case KEY_BACKSPACE:
      if (search_filter_pos == 0) {
        change_mode(MODE_NORMAL);
        break;
      }

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
    logfile_set_filter(app.log, &filter_stringsearch, search_filter);
  } else {
    logfile_set_filter(app.log, NULL, NULL);
  }

  return 1;
}

void print_search_status_line(haywire_state *app, int y, int x) {
  attron(A_BOLD);
  printw(" Filter: ");
  attroff(A_BOLD);
  if (app->log->filter_data != NULL) {
    printw("%s", search_filter);
  }
}

void list_remove_selected() {
  if (app.selected == NULL) {
    return;
  }
  logerror *sel = app.selected;
  
  if (sel->next != NULL) {
    app.selected = sel->next;
  } else if (sel->prev != NULL) {
    app.selected = sel->prev;
  } else {
    app.selected = NULL;
    app.scroll = 0;
  }
  logfile_remove_error(app.log, sel);
  list_scroll(0);
}

void list_select_change(int direction) {
  logerror *err = app.log->errors;
  int rows = list_row_count(&app);

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
  list_scroll(scroll*list_row_count(&app));
}
void list_scroll(int scroll) {
  app.scroll += scroll;
  int maxscroll = errorlist_count(app.log);
  int rows = list_row_count(&app);
  maxscroll -= rows;
  if (app.scroll > maxscroll) app.scroll = maxscroll;
  if (app.scroll < 0) app.scroll = 0;
  if (app.selected != NULL) {
    int n = get_selected(&app);
    if (n >= app.scroll + rows) select_nth(&app, app.scroll+rows-1);
    if (n < app.scroll) select_nth(&app, app.scroll);
  }
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

  init_screen(&app);
}

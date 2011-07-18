#include "statusline.h"
#include "string.h"

int x = 0;

void clear_statusline(haywire_state *app) {
  x = getmaxx(app->screen);
}
void print_to_status(char *msg, short color) {
  if (msg == NULL) return;
  x -= strlen(msg);
  if (color) attron(COLOR_PAIR(color));
  mvprintw(0,x, msg);
  if (color) attroff(COLOR_PAIR(color));
}

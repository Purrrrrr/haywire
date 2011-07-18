#include "statusline.h"
#include "bell.h"

void toggle_bell_type(haywire_state *app) {
  ++app->bell_type;
  if (app->bell_type > BELLTYPE_MAX) app->bell_type = 0;
}
void toggle_bell_level(haywire_state *app) {
  switch(app->bell_level) {
    case E_PARSE:
    app->bell_level = E_NOTICE; break;
    case E_NOTICE:
    app->bell_level = E_MISSING_FILE; break;
    case E_MISSING_FILE:
    app->bell_level = E_PARSE; break;
  }
}
void ring_bells(haywire_state *app) {
  short bell_ringed = 0;
  logerror *err = app->log->errorlist;

  while(err != NULL) {
    if (err->type <= app->bell_level) {
      if (app->bell_type == BELL_ON_NEW_ERROR && err->date > app->last_error_date) {
        bell_ringed = 1;
      } else if (app->bell_type == BELL_ON_NEW_ERRORTYPE && err->is_new) {
        bell_ringed = 1;
      }
    }
    err->is_new = 0;
    if (err->date > app->last_error_date) app->last_error_date = err->date;
    err = err->next;
  }
  if (bell_ringed) {
    beep();
  }
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

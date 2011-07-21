#include <unistd.h>
#include <stdlib.h>
#include "appstate.h"
#include "logs.h"

haywire_state app;

int main(int argv, char *args[]) {
  
  app = default_state;
  if (!parse_arguments(&app, argv, args)) print_usage();
  
  logfile *log = app.log;
  logerror *err = log->errorlist;

  size_t filename_len = 256;
  char *filename = malloc(filename_len);

  while(err != NULL) {
    logerror_nicepath(err, app.relative_path, &filename, &filename_len);

    printf("%d %s: %s:%d (%d times)\n", err->type, err->msg, filename, err->linenr, err->count);
    err = err->next;
  } 

  logfile_close(log);
  exit(EXIT_SUCCESS);

}



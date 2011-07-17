#include "appstate.h"
#include <unistd.h>

int parse_arguments(haywire_state *state, int argv, char *args[]) {
  if (argv < 2) {
    return 0;
  }

  state->relative_path = getcwd(NULL,0);

  //Initial log reading
  logfile *log = logfile_create();
  if (log == NULL) exit(EXIT_FAILURE);

  int read_lines = 10;
  int read_additional_filenames = 0;
  char *filename = NULL;

  for(int a = 1; a < argv; ++a) {
    char *arg = args[a];
    if (arg[0] == '-') {
      switch(arg[1]) {
        case 'n':
          ++a;
          if (!sscanf(args[a], "%d", &read_lines)) {
            logfile_close(log);
            return 0;
          }
          break;
        case 'w':
          read_lines = READ_ALL_LINES;
          break;
        case 'a':
          if (filename == NULL) {
            logfile_close(log);
            return 0;
          }
          read_additional_filenames = 1;
          break;
      }
    } else if (filename == NULL) {
      filename = arg;
    } else if (read_additional_filenames) {
      if (!logfile_add_file(log, arg, READ_ALL_LINES)) {
        printf("Error opening file %s\n", arg);
        exit(EXIT_FAILURE);
      }
    } else {
      logfile_close(log);
      return 0;
    }
  }
  if (filename == NULL) {
    logfile_close(log);
    return 0;
  }
  if (!logfile_add_file(log, filename, read_lines)) {
    printf("Error opening file %s\n", filename);
    exit(EXIT_FAILURE);
  }

  state->log = log;
  
  return 1;
}

void print_usage() {
  printf("Usage: haywire [-w|-n lines] filename [-a filename [filename] ...]\n");
  printf("-n Determines how many last lines are included from the file.\n");
  printf("   The default is 10.\n");
  printf("-w Read the whole file\n");
  printf("-a Tells the analyzer to append an additional file to the analysis without following it\n");
  exit(EXIT_SUCCESS);
}


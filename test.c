#include <curses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "logs.h"

int update_delay = 10;

void usage();
void init();
void vim();

int main(int argv, char *args[]) {
  if (argv < 2) {
    usage();
  }
  
  //Initial log reading
  logfile *log = logfile_create();
  int read_lines = 10;
  int read_additional_filenames = 0;
  char *filename = NULL;

  for(int a = 1; a < argv; ++a) {
    char *arg = args[a];
    if (arg[0] == '-') {
      switch(arg[1]) {
        case 'n':
          ++a;
          if (!sscanf(args[a], "%d", &read_lines)) usage();
          break;
        case 'w':
          read_lines = READ_ALL_LINES;
          break;
        case 'a':
          if (filename == NULL) usage();
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
      usage();
    }
  }
  if (filename == NULL) {
    usage();
  }
  if (!logfile_add_file(log, filename, read_lines)) {
    printf("Error opening file %s\n", filename);
    exit(EXIT_FAILURE);
  }
  
  logerror **errors;
  logerror *err;
  errors = logfile_get_errors(log);
  int i = 0;

  while(errors[i] != NULL) {
    err = errors[i];
    printf("%d %s in %s:%d (%d times)\n", err->type, err->msg, err->filename, err->linenr, err->count);
    ++i;
  }


  logfile_close(log);
  exit(EXIT_SUCCESS);

  //Init display
  init();

  while(1) {
//    mvaddchstr(0,0,"Test");
    int c = getch();
    if (c == 'q') break;
    //if (c == 'v') vim();
    move(1,0);
    addch(c);
    refresh();
  }

  endwin();

}

void usage() {
  printf("Usage: monifail [-w|-n lines] filename [-a filename [filename] ...]\n");
  printf("-n Determines how many last lines are included from the file.\n");
  printf("   The default is 10.\n");
  printf("-w Read the whole file");
  printf("-a Tells the analyzer to append an additional file to the analysis without following it\n");
  exit(EXIT_SUCCESS);
}

void init() {
  WINDOW *win = initscr();
  noecho();
  nonl();
  halfdelay(update_delay);
  clear();

  mvaddstr(0,0,"Testing...");
  refresh();
}

void vim() {
  pid_t pid = fork();

  if (pid == 0) {
    endwin();
    execl("/usr/bin/vim", "vim", NULL );
    exit(EXIT_FAILURE);
  } else {
    int status = 0;
    wait(&status);
    init();
  }
}

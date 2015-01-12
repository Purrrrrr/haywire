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
#include <stdlib.h>
#include "apacheLogParser.h"
#include "appstate.h"
#include "loglist.h"

haywire_state app;

int main(int argv, char *args[]) {
  app = default_state;

  printf("format: \"%s\"\n","[%t] [%l] [client\\ %a] %M% ,referer\\ %{referer}i");

  if (!parse_arguments(&app, argv, args)) print_usage();

  logParseToken *a = app.log->logformat;
  printf("------------------\n");
  while(a != NULL) {
    printf("before: \"%s\"\n",a->string_before);
    printf("type: %d\n",a->type);
    printf("after: \"%s\"\n",a->string_after);
    a = a->next;
    printf("------------------\n");
  }
  
  logfile *log = app.log;
  logerror *err = log->errors;

  size_t filename_len = 256;
  char *filename = malloc(filename_len);

  while(err != NULL) {
    logerror_nicepath(err, app.relative_path, &filename, &filename_len);

    printf("%d %s: %s:%d (%d times)\n", err->type, err->msg, filename, err->linenr, err->count);
    logerror_occurrence *occ = err->latest_occurrence;
    while(0 && occ != NULL) {
      printf("\t%s", get_log_time(occ));
      if (occ->referer != NULL) {
        printf(" referer: %s", occ->referer);
      }
      if (occ->stack_trace != NULL) {
        printf("\n Stack trace: %s", occ->stack_trace);
      }
      printf("\n");
      occ = occ->prev;
    }

    err = err->next;
  } 

  logfile_close(log);
  exit(EXIT_SUCCESS);

}



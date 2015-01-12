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

#include "linereader.h"
#define SEEK_BUFFER_SIZE 1024
void linereader_seek(FILE *logfile, int initial_row_count);

linereader *linereader_open(char *filename, int initial_row_count) {
  FILE *file = NULL;
  linereader *reader;
  
  file = fopen(filename, "r");
  if (file == NULL) return NULL;
  
  reader = malloc(sizeof(linereader));
  if (reader == NULL) {
    fclose(file);
    return NULL;
  }
  linereader_seek(file, initial_row_count);
  reader->file = file;
  reader->buffer = NULL;
  reader->bufferlength = 0;
  
  return reader;
}
void linereader_seek(FILE *logfile, int initial_row_count) {
  if (initial_row_count == READ_ALL_LINES) {
    fseeko(logfile, 0, SEEK_SET);
    return;
  }
  fseeko(logfile, 0, SEEK_END);
  if (initial_row_count == 0) return;

  long pos_in_buffer = 0;
  int row_count = 0;
  char *buffer;
  buffer = malloc(sizeof(char) * SEEK_BUFFER_SIZE);

  //Rewind a bit
  int read;
  off_t buffer_pos = ftello(logfile);
  if (buffer_pos == 0) return;
  off_t seek = buffer_pos % SEEK_BUFFER_SIZE;
  buffer_pos -= seek != 0 ? seek : SEEK_BUFFER_SIZE;
  fseeko(logfile, buffer_pos, SEEK_SET);

  while((read = fread(buffer, sizeof(char), SEEK_BUFFER_SIZE, logfile)) > 0) {

    for(pos_in_buffer = read-1; pos_in_buffer >= 0; --pos_in_buffer) {
      if (buffer[pos_in_buffer] != '\n') continue;
      ++row_count;
      if (row_count > initial_row_count) goto end; //The same as two breaks
    }

    if (buffer_pos == 0) {
      break;
    } else {
      buffer_pos -= SEEK_BUFFER_SIZE;
      fseeko(logfile, buffer_pos, SEEK_SET);
    }
  }
  
  //We goto here from the for loop that inspects the buffer.
  end:

  //If not enough rows, then the file is already seeked properly to the beginning
  if (row_count <= initial_row_count) {
    fseeko(logfile, 0, SEEK_SET);
  }
  
  fseeko(logfile, buffer_pos+pos_in_buffer+1, SEEK_SET);
  free(buffer);
}
char *linereader_getline(linereader *r) {
  ssize_t len = getline(&r->buffer, &r->bufferlength, r->file);
  
  if (len < 0) return NULL;

  //We don't like 'em trailin newlines!
  if (len >= 1 && r->buffer[len-1] == '\n') {
    r->buffer[len-1] = '\0';
  }

  return r->buffer;
}
void linereader_close(linereader *r) {
  fclose(r->file);
  free(r->buffer);
  free(r);
}

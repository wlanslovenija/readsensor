/*
 * Debug sensor reader.
 *
 * Copyright (C) 2012 by Jernej Kos <k@jst.sm>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

void show_help(const char *app)
{
  fprintf(stderr, "usage: %s [options]\n", app);
  fprintf(stderr,
    "       -h         this text\n"
    "       -i sensor  sensor identifier\n"
    "       -d device  serial device\n"
    "       -t timeout wanted timeout in ms (default = 100ms)\n"
    "       -s value   write value to sensor\n"
  );
}

int main(int argc, char **argv)
{
  // Parse program options
  char *sensor_id = NULL;
  char *device = NULL;
  char *aset = NULL;
  unsigned short timeout = 100;
  
  char c;
  while ((c = getopt(argc, argv, "hi:d:s:t:")) != EOF) {
    switch (c) {
      case 'h': {
        show_help(argv[0]);
        return 1;
      }
      case 'i': sensor_id = strdup(optarg); break;
      case 'd': device = strdup(optarg); break;
      case 's': aset = strdup(optarg); break;
      case 't': timeout = atoi(optarg); break;
      default: {
        fprintf(stderr, "ERROR: Invalid option %c!\n", c);
        show_help(argv[0]);
        return 1;
      }
    }
  }
  
  if (sensor_id == NULL || device == NULL) {
    fprintf(stderr, "ERROR: Sensor identifier and serial device path are required!\n");
    return 1;
  }
  
  // Open the serial device
  FILE *serial = fopen(device, "r+");
  if (!serial) {
    fprintf(stderr, "ERROR: Failed to open the serial device '%s'!\n", device);
    return 2;
  }

  // Setup the serial device for non-blocking mode
  int serial_fd = fileno(serial);
  if (fcntl(serial_fd, F_SETFL, O_NONBLOCK) < 0) {
    fprintf(stderr, "ERROR: Failed to setup the serial device!\n");
    return 2;
  }
  
  char buffer[1024] = {0,};
  
  if (aset != NULL) {
    // Send write command instead of read
    if (fprintf(serial, "ACOM /%s %s\n", sensor_id, aset) < 0) {
      fprintf(stderr, "ERROR: Failed to send ASET %s command!\n", aset);
      fclose(serial);
      return 3;
    }
  } else {
    // Send read command
    if (fprintf(serial, "ACOM /%s\n", sensor_id) < 0) {
      fprintf(stderr, "ERROR: Failed to send AGET command!\n");
      fclose(serial);
      return 3;
    }
  }
  
  // Wait on the file descriptor until it is ready
  struct timespec tp_start;
  if (clock_gettime(CLOCK_MONOTONIC, &tp_start) < 0) {
    fprintf(stderr, "ERROR: Failed to get monotonic clock!\n");
    return 3;
  }
  
  char *buffer_idx = (char*) &buffer;
  int state_end = 0;
  
  for (;;) {
    fd_set rfds;
    struct timeval tv;
    
    FD_ZERO(&rfds);
    FD_SET(serial_fd, &rfds);
    
    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    
    int r = select(serial_fd + 1, &rfds, NULL, NULL, &tv);
    if (r == -1) {
      fprintf(stderr, "ERROR: Failed to receive sensor response!\n");
      fclose(serial);
      return 4;
    } else if (r) {
      // Data is ready, read a character
      char c;
      int rr = read(serial_fd, &c, sizeof(char));
      if (rr < 0) {
        fprintf(stderr, "ERROR: Failed to receive sensor response!\n");
        fclose(serial);
        return 4;
      } else if (rr == 0) {
        // End of file
        fprintf(stderr, "ERROR: End of file encountered!\n");
        fclose(serial);
        return 4;
      }
      
      // Finished reading
      if (c == 10) {
        if (state_end == 1)
          break;
        
        state_end++;
        continue;
      }
      
      if (c == 13)
        break;
      
      // Keep reading if we are out of buffer
      if (buffer_idx - (char*) &buffer >= sizeof(buffer))
        continue;
      
      *buffer_idx = c;
      buffer_idx++;
    } else {
      // Nothing to read, check if we have timed out
      struct timespec tp_current;
      if (clock_gettime(CLOCK_MONOTONIC, &tp_current) < 0) {
        fprintf(stderr, "ERROR: Failed to get monotonic clock!\n");
        fclose(serial);
        return 3;
      }
      
      long delta = (tp_current.tv_sec * 1000 + tp_current.tv_nsec / 1000000) -
                   (tp_start.tv_sec * 1000 + tp_start.tv_nsec / 1000000);
      if (delta >= timeout) {
        fprintf(stderr, "ERROR: Timed out while parsing sensor response!\n");
        fclose(serial);
        return 4;
      }
    }
  }
  
  // Output the reported value 
  printf("%s", buffer);
  
  // Cleanup and exit
  fclose(serial);
  return 0;
}

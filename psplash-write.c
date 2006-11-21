/* 
 *  pslash - a lightweight framebuffer splashscreen for embedded devices. 
 *
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  Parts of this file based on 'usplash' copyright Matthew Garret.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "psplash.h"

int main(int argc, char **argv) 
{
  char *tmpdir;
  int   pipe_fd;

  tmpdir = getenv("TMPDIR");

  if (!tmpdir)
    tmpdir = "/tmp";

  if (argc!=2) 
    {
      fprintf(stderr, "Wrong number of arguments\n");
      exit(-1);
    }
  
  chdir(tmpdir);
  
  if ((pipe_fd = open (PSPLASH_FIFO,O_WRONLY|O_NONBLOCK)) == -1)
    {
      /* Silently error out instead of covering the boot process in 
         errors when psplash has exitted due to a VC switch */
      /* perror("Error unable to open fifo"); */
      exit (-1);
    }

  write(pipe_fd, argv[1], strlen(argv[1])+1);

  return 0;
}


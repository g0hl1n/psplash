/* 
 *  pslash - a lightweight framebuffer splashscreen for embedded devices. 
 *
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  Parts of this file ( fifo handling ) based on 'usplash' copyright 
 *  Matthew Garret.
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

#include "psplash.h"
#include "psplash-image.h"

void
psplash_exit (int signum)
{
  DBG("mark");

  psplash_console_reset ();
}

void
psplash_draw_progress (PSplashFB *fb, int value)
{
  int x, y, width, height;

  x      = fb->width/8;
  y      = fb->height - (fb->height/8); 
  width  = fb->width - (fb->width/4);
  height = 16;

  value = CLAMP(value,0,100);

  DBG("total w: %i, val :%i, w: %i", 
      width, value, (value * width) / 100);

  psplash_fb_draw_rect (fb, x, y, width, height, 0xce, 0xce, 0xce);
  psplash_fb_draw_rect (fb, x, y, 
			(value * width) / 100, 
			height, 0x5b, 0x8a, 0xb2);
}

static int 
parse_command (PSplashFB *fb, char *string, int length) 
{
  char *command;
  int   parsed=0;

  parsed = strlen(string)+1;

  DBG("got cmd %s", string);
	
  if (strcmp(string,"QUIT") == 0)
    return 1;

  command = strtok(string," ");

  if (!strcmp(command,"TEXT")) 
    {
      char *line   = strtok(NULL,"\0");
      int   length = strlen(line);		

      while (length>50) 
	{
	  // draw_text(line,50);
	  line+=50;
	  length-=50;
	}

      // draw_text(line,length);
    } 
  else if (!strcmp(command,"STATUS")) 
    {
      // draw_status(strtok(NULL,"\0"),0);
    } 
  else if (!strcmp(command,"SUCCESS")) 
    {
      // draw_status(strtok(NULL,"\0"),TEXT_FOREGROUND);
    } 
  else if (!strcmp(command,"FAILURE")) 
    {
      // draw_status(strtok(NULL,"\0"),RED);
    } 
  else if (!strcmp(command,"PROGRESS")) 
    {
      psplash_draw_progress (fb, atoi(strtok(NULL,"\0")));
      // draw_progress(atoi(strtok(NULL,"\0")));
    } 
  else if (!strcmp(command,"CLEAR")) 
    {
      // text_clear();
    } 
  else if (!strcmp(command,"TIMEOUT")) 
    {
      // timeout=(atoi(strtok(NULL,"\0")));
    } 
  else if (!strcmp(command,"QUIT")) 
    {
      return 1;
    }

  return 0;
}

void 
psplash_main (PSplashFB *fb, int pipe_fd, int timeout) 
{
  int            err;
  ssize_t        length = 0;
  fd_set         descriptors;
  struct timeval tv;
  char          *end;
  char           command[2048];

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  FD_ZERO(&descriptors);
  FD_SET(pipe_fd, &descriptors);

  end = command;

  while (1) 
    {
      if (timeout != 0) 
	err = select(pipe_fd+1, &descriptors, NULL, NULL, &tv);
      else
	err = select(pipe_fd+1, &descriptors, NULL, NULL, NULL);
      
      if (err <= 0) 
	{
	  /*
	  if (errno == EINTR)
	    continue;
	  */
	  return;
	}
      
      length += read (pipe_fd, end, sizeof(command) - (end - command));

      if (length == 0) 
	{
	  /* Reopen to see if there's anything more for us */
	  close(pipe_fd);
	  pipe_fd = open(PSPLASH_FIFO,O_RDONLY|O_NONBLOCK);
	  goto out;
	}
      
      if (command[length-1] == '\0') 
	{
	  if (parse_command(fb, command, strlen(command))) 
	    return;
	  length = 0;
	} 
    out:
      end = &command[length];
    
      tv.tv_sec = timeout;
      tv.tv_usec = 0;
      
      FD_ZERO(&descriptors);
      FD_SET(pipe_fd,&descriptors);
    }

  return;
}

int 
main (int argc, char** argv) 
{
  char      *tmpdir;
  int        pipe_fd;
  PSplashFB *fb;

  signal(SIGHUP, psplash_exit);
  signal(SIGINT, psplash_exit);
  signal(SIGQUIT, psplash_exit);

  psplash_console_switch ();

  tmpdir = getenv("TMPDIR");

  if (!tmpdir)
    tmpdir = "/tmp";

  chdir(tmpdir);

  if (mkfifo(PSPLASH_FIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))
    {
      if (errno!=EEXIST) 
	{
	  perror("mkfifo");
	  exit(-1);
	}
    }

  pipe_fd = open (PSPLASH_FIFO,O_RDONLY|O_NONBLOCK);
  
  if (pipe_fd==-1) 
    {
      perror("pipe open");
      exit(-2);
    }

  if ((fb = psplash_fb_new()) == NULL)
    exit(-1);

  DBG("rect to %ix%i", fb->width, fb->height);

  psplash_fb_draw_rect (fb, 0, 0, fb->width, fb->height, 0xff, 0xff, 0xff);

  psplash_fb_draw_image (fb, 
			 (fb->width  - IMG_WIDTH)/2, 
			 (fb->height - IMG_HEIGHT)/4, 
			 IMG_WIDTH,
			 IMG_HEIGHT,
			 IMG_BYTES_PER_PIXEL,
			 IMG_RLE_PIXEL_DATA);

  psplash_draw_progress (fb, 50);

  psplash_main (fb, pipe_fd, 0);

  psplash_console_reset ();

  psplash_fb_destroy (fb);

  return 0;
}

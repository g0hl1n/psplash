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
#include "psplash-config.h"
#include "psplash-colors.h"
#include "psplash-poky-img.h"
#include "psplash-bar-img.h"
#include "radeon-font.h"

#define SPLIT_LINE_POS(fb)                                  \
	(  (fb)->height                                     \
	 - ((  PSPLASH_IMG_SPLIT_DENOMINATOR                \
	     - PSPLASH_IMG_SPLIT_NUMERATOR)                 \
	    * (fb)->height / PSPLASH_IMG_SPLIT_DENOMINATOR) \
	)

void
psplash_exit (int UNUSED(signum))
{
  DBG("mark");

  psplash_console_reset ();
}

void
psplash_draw_msg (PSplashFB *fb, const char *msg)
{
  int w, h;

  psplash_fb_text_size (&w, &h, &radeon_font, msg);

  DBG("displaying '%s' %ix%i\n", msg, w, h);

  /* Clear */

  psplash_fb_draw_rect (fb, 
			0, 
			SPLIT_LINE_POS(fb) - h, 
			fb->width,
			h,
			PSPLASH_BACKGROUND_COLOR);

  psplash_fb_draw_text (fb,
			(fb->width-w)/2, 
			SPLIT_LINE_POS(fb) - h,
			PSPLASH_TEXT_COLOR,
			&radeon_font,
			msg);
}

void
psplash_draw_progress (PSplashFB *fb, int value)
{
  int x, y, width, height, barwidth;

  /* 4 pix border */
  x      = ((fb->width  - BAR_IMG_WIDTH)/2) + 4 ;
  y      = SPLIT_LINE_POS(fb) + 4;
  width  = BAR_IMG_WIDTH - 8; 
  height = BAR_IMG_HEIGHT - 8;

  if (value > 0)
    {
      barwidth = (CLAMP(value,0,100) * width) / 100;
      psplash_fb_draw_rect (fb, x + barwidth, y, 
    			width - barwidth, height,
			PSPLASH_BAR_BACKGROUND_COLOR);
      psplash_fb_draw_rect (fb, x, y, barwidth,
			    height, PSPLASH_BAR_COLOR);
    }
  else
    {
      barwidth = (CLAMP(-value,0,100) * width) / 100;
      psplash_fb_draw_rect (fb, x, y, 
    			width - barwidth, height,
			PSPLASH_BAR_BACKGROUND_COLOR);
      psplash_fb_draw_rect (fb, x + width - barwidth,
			    y, barwidth, height,
			    PSPLASH_BAR_COLOR);
    }

  DBG("value: %i, width: %i, barwidth :%i\n", value, 
		width, barwidth);
}

static int 
parse_command (PSplashFB *fb, char *string)
{
  char *command;

  DBG("got cmd %s", string);
	
  if (strcmp(string,"QUIT") == 0)
    return 1;

  command = strtok(string," ");

  if (!strcmp(command,"PROGRESS")) 
    {
      psplash_draw_progress (fb, atoi(strtok(NULL,"\0")));
    } 
  else if (!strcmp(command,"MSG")) 
    {
      psplash_draw_msg (fb, strtok(NULL,"\0"));
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
	  if (parse_command(fb, command))
	    return;
	  length = 0;
	} 
      else if (command[length-1] == '\n') 
	{
	  command[length-1] = '\0';
	  if (parse_command(fb, command))
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
  int        pipe_fd, i = 0, angle = 0, fbdev_id = 0, ret = 0;
  PSplashFB *fb;
  bool       disable_console_switch = FALSE;

  signal(SIGHUP, psplash_exit);
  signal(SIGINT, psplash_exit);
  signal(SIGQUIT, psplash_exit);

  while (++i < argc) {
    if (!strcmp(argv[i],"-n") || !strcmp(argv[i],"--no-console-switch"))
      {
        disable_console_switch = TRUE;
        continue;
      }

    if (!strcmp(argv[i],"-a") || !strcmp(argv[i],"--angle"))
      {
        if (++i >= argc) goto fail;
        angle = atoi(argv[i]);
        continue;
      }

    if (!strcmp(argv[i],"-f") || !strcmp(argv[i],"--fbdev"))
      {
        if (++i >= argc) goto fail;
        fbdev_id = atoi(argv[i]);
        continue;
      }

    fail:
      fprintf(stderr, 
              "Usage: %s [-n|--no-console-switch][-a|--angle <0|90|180|270>][-f|--fbdev <0..9>]\n", 
              argv[0]);
      exit(-1);
  }

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

  if (!disable_console_switch)
    psplash_console_switch ();

  if ((fb = psplash_fb_new(angle,fbdev_id)) == NULL)
    {
	  ret = -1;
	  goto fb_fail;
    }

  /* Clear the background with #ecece1 */
  psplash_fb_draw_rect (fb, 0, 0, fb->width, fb->height,
                        PSPLASH_BACKGROUND_COLOR);

  /* Draw the Poky logo  */
  psplash_fb_draw_image (fb, 
			 (fb->width  - POKY_IMG_WIDTH)/2, 
#if PSPLASH_IMG_FULLSCREEN
			 (fb->height - POKY_IMG_HEIGHT)/2,
#else
			 (fb->height * PSPLASH_IMG_SPLIT_NUMERATOR
			  / PSPLASH_IMG_SPLIT_DENOMINATOR - POKY_IMG_HEIGHT)/2,
#endif
			 POKY_IMG_WIDTH,
			 POKY_IMG_HEIGHT,
			 POKY_IMG_BYTES_PER_PIXEL,
			 POKY_IMG_ROWSTRIDE,
			 POKY_IMG_RLE_PIXEL_DATA);

  /* Draw progress bar border */
  psplash_fb_draw_image (fb, 
			 (fb->width  - BAR_IMG_WIDTH)/2, 
			 SPLIT_LINE_POS(fb),
			 BAR_IMG_WIDTH,
			 BAR_IMG_HEIGHT,
			 BAR_IMG_BYTES_PER_PIXEL,
			 BAR_IMG_ROWSTRIDE,
			 BAR_IMG_RLE_PIXEL_DATA);

  psplash_draw_progress (fb, 0);

#ifdef PSPLASH_STARTUP_MSG
  psplash_draw_msg (fb, PSPLASH_STARTUP_MSG);
#endif

  psplash_main (fb, pipe_fd, 0);


  psplash_fb_destroy (fb);

 fb_fail:
  unlink(PSPLASH_FIFO);

  if (!disable_console_switch)
    psplash_console_reset ();

  return ret;
}

/* 
 *  pslash - a lightweight framebuffer splashscreen for embedded devices. 
 *
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
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

void
psplash_fb_destroy (PSplashFB *fb)
{
  if (fb->fd >= 0)
    close (fb->fd);

  free(fb);
}

PSplashFB*
psplash_fb_new (void)
{
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;
  int                      off;
  char                    *fbdev;

  PSplashFB *fb = NULL;

  fbdev = getenv("FBDEV");
  if (fbdev == NULL)
    fbdev = "/dev/fb0";

  if ((fb = malloc (sizeof(PSplashFB))) == NULL)
    {
      perror ("Error no memory");
      goto fail;
    }
  
  memset (fb, 0, sizeof(PSplashFB));

  fb->fd = -1;  

  if ((fb->fd = open (fbdev, O_RDWR)) < 0)
    {
      perror ("Error opening /dev/fb0");
      goto fail;
    }

  if (ioctl (fb->fd, FBIOGET_FSCREENINFO, &fb_fix) == -1
      || ioctl (fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
    {
      perror ("Error getting framebuffer info");
      goto fail;
    }
  
  if (fb_var.bits_per_pixel < 16)
    {
      fprintf(stderr,
	      "Error, no support currently for %i bpp frame buffers\n",
	      fb_var.bits_per_pixel);
    }

  fb->width  = fb_var.xres;
  fb->height = fb_var.yres;
  fb->bpp    = fb_var.bits_per_pixel;
  fb->stride = fb_fix.line_length;
  fb->type   = fb_fix.type;
  fb->visual = fb_fix.visual;

  DBG("width: %i, height: %i, bpp: %i, stride: %i", 
      fb->width, fb->height, fb->bpp, fb->stride);


  fb->base = (char *) mmap ((caddr_t) NULL,
			    /*fb_fix.smem_len */
			    fb->stride * fb->height,
			    PROT_READ|PROT_WRITE,
			    MAP_SHARED,
			    fb->fd, 0);
    
  if (fb->base == (char *)-1) 
    {
      perror("Error cannot mmap framebuffer ");
      goto fail;
    }

  off = (unsigned long) fb_fix.smem_start % (unsigned long) getpagesize();

  fb->data = fb->base + off;

#if 0
  /* FIXME: No support for 8pp as yet  */
  if (visual == FB_VISUAL_PSEUDOCOLOR
      || visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
  {
    static struct fb_cmap cmap;

    cmap.start = 0;
    cmap.len = 16;
    cmap.red = saved_red;
    cmap.green = saved_green;
    cmap.blue = saved_blue;
    cmap.transp = NULL;

    ioctl (fb, FBIOGETCMAP, &cmap);
  }
  
  if (!status)
    atexit (bogl_done);
  status = 2;
#endif

  return fb;

 fail:

  if (fb)
    psplash_fb_destroy (fb);

  return NULL;
}

void
psplash_fb_plot_pixel (PSplashFB    *fb, 
		       int          x, 
		       int          y, 
		       uint8        red,
		       uint8        green,
		       uint8        blue)
{
  int off;

  if (x < 0 || x > fb->width-1 || y < 0 || y > fb->height-1)
    return;

  off = (y * fb->stride) + (x * (fb->bpp >> 3));

  /* FIXME: handle no RGB orderings */
  switch (fb->bpp)
    {
    case 24:
    case 32:
      *(fb->data + off)     = red;
      *(fb->data + off + 1) = green;
      *(fb->data + off + 2) = blue;
      break;
    case 16:
      *(volatile uint16 *) (fb->data + off) 
	= ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
      break;
    default:
      /* depth not supported yet */
      break;
    }

}

void
psplash_fb_draw_rect (PSplashFB    *fb, 
		      int          x, 
		      int          y, 
		      int          width, 
		      int          height,
		      uint8        red,
		      uint8        green,
		      uint8        blue)
{
  int dx, dy;

  for (dy=0; dy < height; dy++)   
    for (dx=0; dx < width; dx++)
      {
	/* FIXME: inline this call */
	psplash_fb_plot_pixel (fb, x+dx, y+dy, red, green, blue); 
      }
}

void
psplash_fb_draw_image (PSplashFB    *fb, 
		       int          x, 
		       int          y, 
		       int          img_width, 
		       int          img_height,
		       int          img_bytes_per_pixel,
		       uint8       *rle_data)
{
  uint8 *p = rle_data;
  int    dx = 0, dy = 0,  total_len;
  unsigned int len;

#if 0
  for (dy=0; dy < img_height; dy++)   
    for (dx=0; dx < img_width; dx++)
      {
	psplash_fb_plot_pixel (fb, x+dx, y+dy, *p, *(p+1), *(p+2)); 
	p += img_bytes_per_pixel;
      }
#endif

  total_len = img_width * img_height * img_bytes_per_pixel;

  /* FIXME: Optimise, check for over runs ... */
  while ((p - rle_data) < total_len)
    {
      len = *(p++);

      if (len & 128)
	{
	  len -= 128;

	  if (len == 0) break;

	  do
	    {
	      psplash_fb_plot_pixel (fb, x+dx, y+dy, *p, *(p+1), *(p+2)); 
	      if (++dx >= img_width) { dx=0; dy++; }
	    }
	  while (--len && (p - rle_data) < total_len);

	  p += img_bytes_per_pixel;
	}
      else
	{
	  if (len == 0) break;

	  do
	    {
	      psplash_fb_plot_pixel (fb, x+dx, y+dy, *p, *(p+1), *(p+2)); 
	      if (++dx >= img_width) { dx=0; dy++; }
	      p += img_bytes_per_pixel;
	    }
	  while (--len && (p - rle_data) < total_len);
	}
    }
}

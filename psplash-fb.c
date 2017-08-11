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

#include <endian.h>
#include "psplash.h"

void
psplash_fb_destroy (PSplashFB *fb)
{
  if (fb->fd >= 0)
    close (fb->fd);

  free(fb);
}

static int
attempt_to_change_pixel_format (PSplashFB *fb,
                                struct fb_var_screeninfo *fb_var)
{
  /* By default the framebuffer driver may have set an oversized
   * yres_virtual to support VT scrolling via the panning interface.
   *
   * We don't try and maintain this since it's more likely that we
   * will fail to increase the bpp if the driver's pre allocated
   * framebuffer isn't large enough.
   */
  fb_var->yres_virtual = fb_var->yres;

  /* First try setting an 8,8,8,0 pixel format so we don't have to do
   * any conversions while drawing. */

  fb_var->bits_per_pixel = 32;

  fb_var->red.offset = 0;
  fb_var->red.length = 8;

  fb_var->green.offset = 8;
  fb_var->green.length = 8;

  fb_var->blue.offset = 16;
  fb_var->blue.length = 8;

  fb_var->transp.offset = 0;
  fb_var->transp.length = 0;

  if (ioctl (fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0)
    {
      fprintf(stdout, "Switched to a 32 bpp 8,8,8 frame buffer\n");
      return 1;
    }
  else
    {
      fprintf(stderr,
              "Error, failed to switch to a 32 bpp 8,8,8 frame buffer\n");
    }

  /* Otherwise try a 16bpp 5,6,5 format */

  fb_var->bits_per_pixel = 16;

  fb_var->red.offset = 11;
  fb_var->red.length = 5;

  fb_var->green.offset = 5;
  fb_var->green.length = 6;

  fb_var->blue.offset = 0;
  fb_var->blue.length = 5;

  fb_var->transp.offset = 0;
  fb_var->transp.length = 0;

  if (ioctl (fb->fd, FBIOPUT_VSCREENINFO, fb_var) == 0)
    {
      fprintf(stdout, "Switched to a 16 bpp 5,6,5 frame buffer\n");
      return 1;
    }
  else
    {
      fprintf(stderr,
              "Error, failed to switch to a 16 bpp 5,6,5 frame buffer\n");
    }

  return 0;
}

PSplashFB*
psplash_fb_new (int angle, int fbdev_id)
{
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;
  int                      off;
  char                     fbdev[9] = "/dev/fb0";

  PSplashFB *fb = NULL;

  if (fbdev_id > 0 && fbdev_id < 10)
    {
        // Conversion from integer to ascii.
        fbdev[7] = fbdev_id + 48;
    }

  if ((fb = malloc (sizeof(PSplashFB))) == NULL)
    {
      perror ("Error no memory");
      goto fail;
    }

  memset (fb, 0, sizeof(PSplashFB));

  fb->fd = -1;

  if ((fb->fd = open (fbdev, O_RDWR)) < 0)
    {
      fprintf(stderr,
              "Error opening %s\n",
              fbdev);
      goto fail;
    }

  if (ioctl (fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
    {
      perror ("Error getting variable framebuffer info");
      goto fail;
    }

  if (fb_var.bits_per_pixel < 16)
    {
      fprintf(stderr,
              "Error, no support currently for %i bpp frame buffers\n"
              "Trying to change pixel format...\n",
              fb_var.bits_per_pixel);
      if (!attempt_to_change_pixel_format (fb, &fb_var))
        goto fail;
    }

  if (ioctl (fb->fd, FBIOGET_VSCREENINFO, &fb_var) == -1)
    {
      perror ("Error getting variable framebuffer info (2)");
      goto fail;
    }

  /* NB: It looks like the fbdev concept of fixed vs variable screen info is
   * broken. The line_length is part of the fixed info but it can be changed
   * if you set a new pixel format. */
  if (ioctl (fb->fd, FBIOGET_FSCREENINFO, &fb_fix) == -1)
    {
      perror ("Error getting fixed framebuffer info");
      goto fail;
    }

  fb->real_width  = fb->width  = fb_var.xres;
  fb->real_height = fb->height = fb_var.yres;
  fb->bpp    = fb_var.bits_per_pixel;
  fb->stride = fb_fix.line_length;
  fb->type   = fb_fix.type;
  fb->visual = fb_fix.visual;

  fb->red_offset = fb_var.red.offset;
  fb->red_length = fb_var.red.length;
  fb->green_offset = fb_var.green.offset;
  fb->green_length = fb_var.green.length;
  fb->blue_offset = fb_var.blue.offset;
  fb->blue_length = fb_var.blue.length;

  if (fb->red_offset == 11 && fb->red_length == 5 &&
      fb->green_offset == 5 && fb->green_length == 6 &&
      fb->blue_offset == 0 && fb->blue_length == 5) {
         fb->rgbmode = RGB565;
  } else if (fb->red_offset == 0 && fb->red_length == 5 &&
      fb->green_offset == 5 && fb->green_length == 6 &&
      fb->blue_offset == 11 && fb->blue_length == 5) {
         fb->rgbmode = BGR565;
  } else if (fb->red_offset == 16 && fb->red_length == 8 &&
      fb->green_offset == 8 && fb->green_length == 8 &&
      fb->blue_offset == 0 && fb->blue_length == 8) {
         fb->rgbmode = RGB888;
  } else if (fb->red_offset == 0 && fb->red_length == 8 &&
      fb->green_offset == 8 && fb->green_length == 8 &&
      fb->blue_offset == 16 && fb->blue_length == 8) {
         fb->rgbmode = BGR888;
  } else {
         fb->rgbmode = GENERIC;
  }

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

  fb->angle = angle;

  switch (fb->angle)
    {
    case 270:
    case 90:
      fb->width  = fb->real_height;
      fb->height = fb->real_width;
      break;
    case 180:
    case 0:
    default:
      break;
    }

  return fb;

 fail:

  if (fb)
    psplash_fb_destroy (fb);

  return NULL;
}

#define OFFSET(fb,x,y) (((y) * (fb)->stride) + ((x) * ((fb)->bpp >> 3)))

static inline void
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

  switch (fb->angle)
    {
    case 270:
      off = OFFSET (fb, fb->height - y - 1, x);
      break;
    case 180:
      off = OFFSET (fb, fb->width - x - 1, fb->height - y - 1);
      break;
    case 90:
      off = OFFSET (fb, y, fb->width - x - 1);
      break;
    case 0:
    default:
      off = OFFSET (fb, x, y);
      break;
    }

  if (fb->rgbmode == RGB565 || fb->rgbmode == RGB888) {
    switch (fb->bpp)
      {
      case 24:
#if __BYTE_ORDER == __BIG_ENDIAN
        *(fb->data + off + 0) = red;
        *(fb->data + off + 1) = green;
        *(fb->data + off + 2) = blue;
#else
        *(fb->data + off + 0) = blue;
        *(fb->data + off + 1) = green;
        *(fb->data + off + 2) = red;
#endif
        break;
      case 32:
        *(volatile uint32_t *) (fb->data + off)
          = (red << 16) | (green << 8) | (blue);
        break;

      case 16:
        *(volatile uint16_t *) (fb->data + off)
	  = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
        break;
      default:
        /* depth not supported yet */
        break;
      }
  } else if (fb->rgbmode == BGR565 || fb->rgbmode == BGR888) {
    switch (fb->bpp)
      {
      case 24:
#if __BYTE_ORDER == __BIG_ENDIAN
        *(fb->data + off + 0) = blue;
        *(fb->data + off + 1) = green;
        *(fb->data + off + 2) = red;
#else
        *(fb->data + off + 0) = red;
        *(fb->data + off + 1) = green;
        *(fb->data + off + 2) = blue;
#endif
        break;
      case 32:
        *(volatile uint32_t *) (fb->data + off)
          = (blue << 16) | (green << 8) | (red);
        break;
      case 16:
        *(volatile uint16_t *) (fb->data + off)
	  = ((blue >> 3) << 11) | ((green >> 2) << 5) | (red >> 3);
        break;
      default:
        /* depth not supported yet */
        break;
      }
  } else {
    switch (fb->bpp)
      {
      case 32:
        *(volatile uint32_t *) (fb->data + off)
	  = ((red >> (8 - fb->red_length)) << fb->red_offset) 
	      | ((green >> (8 - fb->green_length)) << fb->green_offset)
	      | ((blue >> (8 - fb->blue_length)) << fb->blue_offset);
        break;
      case 16:
        *(volatile uint16_t *) (fb->data + off)
	  = ((red >> (8 - fb->red_length)) << fb->red_offset) 
	      | ((green >> (8 - fb->green_length)) << fb->green_offset)
	      | ((blue >> (8 - fb->blue_length)) << fb->blue_offset);
        break;
      default:
        /* depth not supported yet */
        break;
      }
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
	psplash_fb_plot_pixel (fb, x+dx, y+dy, red, green, blue);
}

void
psplash_fb_draw_image (PSplashFB    *fb,
		       int          x,
		       int          y,
		       int          img_width,
		       int          img_height,
		       int          img_bytes_per_pixel,
		       int          img_rowstride,
		       uint8       *rle_data)
{
  uint8       *p = rle_data;
  int          dx = 0, dy = 0,  total_len;
  unsigned int len;

  total_len = img_rowstride * img_height;

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
	      if ((img_bytes_per_pixel < 4 || *(p+3)) && dx < img_width)
	        psplash_fb_plot_pixel (fb, x+dx, y+dy, *(p), *(p+1), *(p+2));
	      if (++dx * img_bytes_per_pixel >= img_rowstride) { dx=0; dy++; }
	    }
	  while (--len);

	  p += img_bytes_per_pixel;
	}
      else
	{
	  if (len == 0) break;

	  do
	    {
	      if ((img_bytes_per_pixel < 4 || *(p+3)) && dx < img_width)
	        psplash_fb_plot_pixel (fb, x+dx, y+dy, *(p), *(p+1), *(p+2));
	      if (++dx * img_bytes_per_pixel >= img_rowstride) { dx=0; dy++; }
	      p += img_bytes_per_pixel;
	    }
	  while (--len && (p - rle_data) < total_len);
	}
    }
}

/* Font rendering code based on BOGL by Ben Pfaff */

static int
psplash_font_glyph (const PSplashFont *font, wchar_t wc, u_int32_t **bitmap)
{
  int mask = font->index_mask;
  int i;

  for (;;)
    {
      for (i = font->offset[wc & mask]; font->index[i]; i += 2)
	{
	  if ((wchar_t)(font->index[i] & ~mask) == (wc & ~mask))
	    {
	      if (bitmap != NULL)
		*bitmap = &font->content[font->index[i+1]];
	      return font->index[i] & mask;
	    }
	}
    }
  return 0;
}

void
psplash_fb_text_size (int                *width,
		      int                *height,
		      const PSplashFont  *font,
		      const char         *text)
{
  char   *c = (char*)text;
  wchar_t wc;
  int     k, n, w, h, mw;

  n = strlen (text);
  mw = h = w = 0;

  mbtowc (0, 0, 0);
  for (; (k = mbtowc (&wc, c, n)) > 0; c += k, n -= k)
    {
      if (*c == '\n')
	{
	  if (w > mw)
	    mw = w;
	  w = 0;
	  h += font->height;
	  continue;
	}

      w += psplash_font_glyph (font, wc, NULL);
    }

  *width  = (w > mw) ? w : mw;
  *height = (h == 0) ? font->height : h;
}

void
psplash_fb_draw_text (PSplashFB         *fb,
		      int                x,
		      int                y,
		      uint8              red,
		      uint8              green,
		      uint8              blue,
		      const PSplashFont *font,
		      const char        *text)
{
  int     h, w, k, n, cx, cy, dx, dy;
  char   *c = (char*)text;
  wchar_t wc;

  n = strlen (text);
  h = font->height;
  dx = dy = 0;

  mbtowc (0, 0, 0);
  for (; (k = mbtowc (&wc, c, n)) > 0; c += k, n -= k)
    {
      u_int32_t *glyph = NULL;

      if (*c == '\n')
	{
	  dy += h;
	  dx  = 0;
	  continue;
	}

      w = psplash_font_glyph (font, wc, &glyph);

      if (glyph == NULL)
	continue;

      for (cy = 0; cy < h; cy++)
	{
	  u_int32_t g = *glyph++;

	  for (cx = 0; cx < w; cx++)
	    {
	      if (g & 0x80000000)
		psplash_fb_plot_pixel (fb, x+dx+cx, y+dy+cy,
				       red, green, blue);
	      g <<= 1;
	    }
	}

      dx += w;
    }
}


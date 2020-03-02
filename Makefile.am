bin_PROGRAMS=psplash psplash-write

AM_CFLAGS = $(GCC_FLAGS) $(EXTRA_GCC_FLAGS) -D_GNU_SOURCE -DFONT_HEADER=\"$(FONT_NAME)-font.h\" -DFONT_DEF=$(FONT_NAME)_font

psplash_SOURCES = psplash.c psplash.h psplash-fb.c psplash-fb.h \
                  psplash-console.c psplash-console.h           \
		  psplash-colors.h psplash-config.h		\
		  psplash-poky-img.h psplash-bar-img.h $(FONT_NAME)-font.h
BUILT_SOURCES = psplash-poky-img.h psplash-bar-img.h

psplash_write_SOURCES = psplash-write.c psplash.h

if HAVE_SYSTEMD
psplash_CPPFLAGS = $(SYSTEMD_CFLAGS) -DHAVE_SYSTEMD
psplash_LDFLAGS= $(SYSTEMD_LIBS)
bin_PROGRAMS += psplash-systemd
psplash_systemd_CPPFLAGS = $(SYSTEMD_CFLAGS)
psplash_systemd_LDFLAGS= $(SYSTEMD_LIBS)
psplash_systemd_SOURCES = psplash-systemd.c psplash.h
endif

EXTRA_DIST = make-image-header.sh
 
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

psplash-bar-img.h: base-images/psplash-bar.png
	$(top_srcdir)/make-image-header.sh $< BAR
psplash-poky-img.h: base-images/psplash-poky.png
	$(top_srcdir)/make-image-header.sh $< POKY


#!/bin/sh
imageh=psplash-image.h
gdk-pixbuf-csource --macros $1 > $imageh.tmp
sed -e 's/MY_PIXBUF/IMG/g' -e "s/guint8/uint8/g" $imageh.tmp > $imageh && rm $imageh.tmp



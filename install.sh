#!/usr/bin/bash

glib-genmarshal \
    --prefix=_thunar_marshal \
    --header thunar-marshal.list >> thunar-marshal.h

glib-genmarshal \
    --prefix=_thunar_marshal \
    --body thunar-marshal.list >> thunar-marshal.c

meson build -Dbuildtype=plain
ninja -C build
sudo ninja -C build install



#!/usr/bin/bash

BASEDIR="$(dirname -- "$(readlink -f -- "$0";)")"

dest=$BASEDIR/build
if [[ -d $dest ]]; then
    rm -rf $dest
fi

glib-genmarshal \
    --prefix=_thunar_marshal \
    --header marshal.list > marshal.h

glib-genmarshal \
    --prefix=_thunar_marshal \
    --body marshal.list > marshal.c

meson build -Dbuildtype=plain
ninja -C build
sudo ninja -C build install

dest=/usr/local/share/applications/
if [[ ! -d $dest ]]; then
    echo "*** create application directory"
    sudo mkdir $dest
fi

dest=/usr/local/share/applications/
if [[ ! -f $dest/fileman.desktop ]]; then
    echo "*** install desktop file"
    sudo cp $BASEDIR/data/fileman.desktop $dest
fi



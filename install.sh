#!/usr/bin/bash

BASEDIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

glib-genmarshal \
    --prefix=_thunar_marshal \
    --header thunar-marshal.list > thunar-marshal.h

glib-genmarshal \
    --prefix=_thunar_marshal \
    --body thunar-marshal.list > thunar-marshal.c

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



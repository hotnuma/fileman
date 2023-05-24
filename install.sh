#!/usr/bin/bash

BASEDIR="$(dirname -- "$(readlink -f -- "$0";)")"

dest=$BASEDIR/build
if [[ -d $dest ]]; then
    rm -rf $dest
fi

meson build -Dbuildtype=plain
ninja -C build
sudo ninja -C build install



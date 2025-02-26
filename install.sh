#!/usr/bin/bash

basedir="$(dirname -- "$(readlink -f -- "$0";)")"
opt_clean=0
buildtype="plain"

while (($#)); do
    case "$1" in
        clean)
        opt_clean=1
        ;;
        -type)
        buildtype="debug"
        ;;
        *)
        ;;
    esac
    shift
done

dest=$basedir/build
if [[ $opt_clean == 1 && -d $dest ]]; then
    rm -rf $dest
fi

meson setup build -Dbuildtype=${buildtype}
ninja -C build
sudo ninja -C build install



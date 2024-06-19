#!/bin/sh

# Make sure we have common
if test ! -f common/gst-autogen.sh;then
    echo "+ Setting up common submodule"
    git submodule init
fi
git submodule update

rm -rf autom4te.cache
rm -f aclocal.m4 ltmain.sh

# autoreconf -f -i -v -I common/m4 $ACLOCAL_FLAGS --warnings=all || exit 1
. common/gst-autogen.sh
. common/flu-autoreconf.sh

flu_autoreconf $@

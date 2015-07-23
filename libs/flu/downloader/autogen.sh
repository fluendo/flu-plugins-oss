#!/bin/sh

# Make sure we have common
if test ! -f common/gst-autogen.sh;then
    echo "+ Setting up common submodule"
    git submodule init
fi
git submodule update

rm -rf autom4te.cache
rm -f aclocal.m4 ltmain.sh

autoreconf -f -i -v --warnings=all || exit 1

if [ -z "$NOCONFIGURE" ]; then
  ./configure "$@"
fi


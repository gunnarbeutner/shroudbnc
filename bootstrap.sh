#!/bin/sh
aclocal --force
libtoolize --copy --force --ltdl
automake --add-missing --force
autoreconf

#!/bin/sh
aclocal --force

if [ -z "$LIBTOOLIZE" ]; then
	if type -P libtoolize &>/dev/null; then
		LIBTOOLIZE=libtoolize
	elif type -P glibtoolize &>/dev/null; then
		LIBTOOLIZE=glibtoolize
	fi
fi

if [ -z "$LIBTOOLIZE" ]; then
	echo "Please install GNU libtool or set the LIBTOOLIZE environment variable."
fi

$LIBTOOLIZE --copy --force --ltdl
automake --gnu --add-missing
autoconf

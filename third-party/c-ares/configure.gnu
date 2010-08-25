#!/bin/sh
cd `dirname $0` && exec ./configure --disable-shared "$@"

uptime module
-------------

This module allows you to participate in the eggheads.org uptime contest: http://uptime.eggheads.org/

How to compile it?
------------------

g++ -g -shared -Wl,-soname,uptime -fPIC -o uptime.so *.cpp

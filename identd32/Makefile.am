pkglibdir=$(exec_prefix)/${MOD_DIR}

pkglib_LTLIBRARIES=libidentd.la
libidentd_la_SOURCES=identd32.cpp
libidentd_la_LIBADD=../src/libsbnc.la ../src/rpc/librpc.la
libidentd_la_LDFLAGS=-version-info 0:0:0 -no-undefined

CC=@CXX@

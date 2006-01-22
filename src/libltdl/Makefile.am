## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

noinst_HEADERS = ltdl.h
noinst_LTLIBRARIES = libltdlc.la

## Make sure these will be cleaned even when they're not built by
## default.
CLEANFILES = libltdlc.la

libltdlc_la_SOURCES = ltdl.c
libltdlc_la_LIBADD = $(LIBADD_DL)

## Because we do not have automatic dependency tracking:
ltdl.lo: ltdl.h
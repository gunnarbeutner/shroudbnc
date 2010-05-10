SUBDIRS=third-party src @BNCTCL_MODULE@ @IDENTD_MODULE@ php
DIST_SUBDIRS=third-party src bnctcl identd32 php

EXTRA_DIST=aclocal.m4 tcl.m4 ssl.conf LICENSE LICENSE.Exceptions README README.ssl README.motd README.lean README.service README.settings README.iface2

ACLOCAL_AMFLAGS=-I m4

install: install-recursive
	@if [ "${exec_prefix}" = "${HOME}/sbnc" ]; then \
		ln -sf bin/sbnc ${exec_prefix}/sbnc; \
	fi;

sslcert:
	@if [ -f ${exec_prefix}/sbnc.key ]; then \
		cp ${exec_prefix}/sbnc.key ${exec_prefix}/sbnc_old.key; \
	fi;

	@if [ -f ${exec_prefix}/sbnc.crt ]; then \
		cp ${exec_prefix}/sbnc.crt ${exec_prefix}/sbnc_old.crt; \
	fi;

	@if [ ! -d ${exec_prefix} ]; then \
		echo "Please install shroudBNC first."; \
	else \
		openssl req -new -x509 -days 3650 -nodes -keyout ${exec_prefix}/sbnc.key -out ${exec_prefix}/sbnc.crt -config ssl.conf; \
	fi;

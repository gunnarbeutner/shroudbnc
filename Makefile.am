if BUILD_BNCTCL_MODULE
BNCTCL_MODULE=bnctcl
endif

if BUILD_IDENTD_MODULE
IDENTD_MODULE=bncidentd
endif

SUBDIRS=third-party src $(BNCTCL_MODULE) $(IDENTD_MODULE) php

EXTRA_DIST=aclocal.m4 debian ssl.conf LICENSE LICENSE.Exceptions m4/tcl.m4 README.md README.copyright README.faq README.iface2 README.lean README.motd README.settings README.ssl sbnc-start sbnc_version.h

ACLOCAL_AMFLAGS=-I m4

install: install-recursive
	@if [ "${exec_prefix}" = "${HOME}/sbnc" ]; then \
		${INSTALL_PROGRAM} sbnc-start ${exec_prefix}/sbnc; \
	fi;

sslcert:
	@if [ "${exec_prefix}" != "${HOME}/sbnc" ]; then \
		echo "make sslcert can only be used when installing shroudBNC in your home directory. Please use openssl instead to create your SSL certificates."; \
		exit 1; \
	fi;

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

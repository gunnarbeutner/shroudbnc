export MOD_DIR=@SUBST_MOD_DIR@
SUBDIRS=src @BNCTCL_MODULE@ @IDENTD_MODULE@
EXTRA_DIST=cares.m4 cares2.m4 aclocal.m4 tcl.m4 sbnc.tcl ssl.conf README README.ssl README.motd README.lean README.service README.settings

install:
	@echo ./${MOD_DIR}/libsbnc.la > ${exec_prefix}/sbnc.ipc
	@if [ ! -f ${exec_prefix}/sbnc.tcl ]; then cp sbnc.tcl ${exec_prefix}/sbnc.tcl; fi
	@chmod 600 ${exec_prefix}/sbnc.tcl

update:
	@export MOD_DIR="lib-`date +%Y%m%d%H%M%S`"; \
	${MAKE} clean; \
	${MAKE} -C src; \
	${MAKE} -C bnctcl; \
	${MAKE} -C src install-exec-am; \
	${MAKE} -C bnctcl install; \
	${MAKE} -C scripts install; \
	echo "Reloading any running instances of shroudBNC..."; \
	echo ./$${MOD_DIR}/libsbnc.la > ${exec_prefix}/sbnc.ipc;
	@if [ -f ${exec_prefix}/sbnc.pid ]; then \
		if $$(kill -CHLD `cat ${exec_prefix}/sbnc.pid` >/dev/null 2>&1); then \
			echo "Sending USR1 signal to shroudBNC instance in ${exec_prefix} (PID `cat ${exec_prefix}/sbnc.pid`)"; \
			kill -USR1 `cat ${exec_prefix}/sbnc.pid` >/dev/null 2>&1; \
		fi; \
	fi;
	@echo "********************************************"
	@echo "* shroudBNC has successfully been updated. *"
	@echo "********************************************"

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

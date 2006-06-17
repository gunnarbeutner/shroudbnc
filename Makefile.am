export MOD_DIR=@SUBST_MOD_DIR@
SUBDIRS=src @BNCTCL_MODULE@ @IDENTD_MODULE@
EXTRA_DIST=cares.m4 cares2.m4 aclocal.m4 tcl.m4 sbnc.tcl ca.crt ca.key sbnc.crt sbnc.key README README.ssl README.motd README.lean README.service

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
		echo "Sending USR1 signal to shroudBNC instance in ${exec_prefix} (PID `cat ${exec_prefix}/sbnc.pid`)"; \
		kill -USR1 `cat ${exec_prefix}/sbnc.pid` >/dev/null 2>&1; \
	fi;
	@echo "********************************************"
	@echo "* shroudBNC has successfully been updated. *"
	@echo "********************************************"

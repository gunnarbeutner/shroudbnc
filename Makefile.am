export MOD_DIR=@SUBST_MOD_DIR@
SUBDIRS=src @BNCTCL_MODULE@ @IDENTD_MODULE@
EXTRA_DIST=cares.m4 cares2.m4 aclocal.m4 tcl.m4 sbnc.tcl ca.crt ca.key sbnc.crt sbnc.key README README.ssl README.motd
UPDATE_MOD_DIR="lib-`date +%Y%m%d%H%M%S`"

install:
	@echo ./${MOD_DIR}/libsbnc.la > ${exec_prefix}/sbnc.ipc
	@if [ ! -f ${exec_prefix}/sbnc.tcl ]; then cp sbnc.tcl ${exec_prefix}/sbnc.tcl; fi
	@chmod 600 ${exec_prefix}/sbnc.tcl

update:
	@MOD_DIR=${UPDATE_MOD_DIR} ${MAKE} -C bnctcl install
	@MOD_DIR=${UPDATE_MOD_DIR} ${MAKE} -C scripts install
	@MOD_DIR=${UPDATE_MOD_DIR} ${MAKE} -C src install-exec-am
	@echo "Reloading any running instances of shroudBNC..."
	@echo ./${UPDATE_MOD_DIR}/libsbnc.la > ${exec_prefix}/sbnc.ipc
	@if [ -f ${exec_prefix}/sbnc.pid ]; then \
		@echo "Sending USR1 signal to shroudBNC instance in ${exec_prefix}/sbnc.pid (PID `cat ${exec_prefix}/sbnc.pid`)"; \
		kill -USR1 `cat ${exec_prefix}/sbnc.pid` >/dev/null 2>&1; \
	fi;
	@echo "********************************************"
	@echo "* shroudBNC has successfully been updated. *"
	@echo "********************************************"

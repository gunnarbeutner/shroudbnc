export MOD_DIR=@SUBST_MOD_DIR@
SUBDIRS=src @BNCTCL_MODULE@
EXTRA_DIST=cares.m4 cares2.m4 aclocal.m4 tcl.m4 sbnc.tcl ca.crt ca.key sbnc.crt sbnc.key README README.ssl README.motd

install:
	echo ./${MOD_DIR}/libsbnc.la > ${exec_prefix}/sbnc.ipc

update:
	MOD_DIR=lib-1 ${MAKE} -C bnctcl install
	MOD_DIR=lib-1 ${MAKE} -C src install-exec-am
	echo ./${MOD_DIR}/libsbnc.la > ${exec_prefix}/sbnc.ipc
	if [ -f ${HOME}/sbnc/sbnc.pid ]; then kill -USR1 `cat ${HOME}/sbnc/sbnc.pid`; fi
	@echo "Done."

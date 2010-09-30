if USE_BUILTIN_CARES
BUILTIN_CARES=c-ares	
endif

if USE_BUILTIN_SNPRINTF
BUILTIN_SNPRINTF=snprintf
endif

SUBDIRS=libltdl $(BUILTIN_CARES) mmatch md5 $(BUILTIN_SNPRINTF)

EXTRA_DIST=c-ares/configure.gnu

install:

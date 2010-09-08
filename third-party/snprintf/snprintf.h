#ifndef _PORTABLE_SNPRINTF_H_
#define _PORTABLE_SNPRINTF_H_

#define PORTABLE_SNPRINTF_VERSION_MAJOR 2
#define PORTABLE_SNPRINTF_VERSION_MINOR 2

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_SNPRINTF
#include <stdio.h>
#else
extern int snprintf(char *, size_t, const char *, /*args*/ ...);
extern int vsnprintf(char *, size_t, const char *, va_list);
#endif

#if defined(HAVE_SNPRINTF) && defined(PREFER_PORTABLE_SNPRINTF)
extern int portable_snprintf(char *str, size_t str_m, const char *fmt, /*args*/ ...);
extern int portable_vsnprintf(char *str, size_t str_m, const char *fmt, va_list ap);
#define snprintf  portable_snprintf
#define vsnprintf portable_vsnprintf
#endif

extern SBNCAPI int asprintf  (char **ptr, const char *fmt, /*args*/ ...);
extern SBNCAPI int vasprintf (char **ptr, const char *fmt, va_list ap);
extern SBNCAPI int asnprintf (char **ptr, size_t str_m, const char *fmt, /*args*/ ...);
extern SBNCAPI int vasnprintf(char **ptr, size_t str_m, const char *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif

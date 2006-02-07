dnl check for a few basic system headers we need
AC_CHECK_HEADERS(
       sys/types.h \
       sys/time.h \
       sys/select.h \
       sys/socket.h \
       sys/ioctl.h \
       winsock.h \
       netinet/in.h \
       net/if.h \
       arpa/nameser.h \
       arpa/nameser_compat.h \
       arpa/inet.h, , ,
[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
dnl We do this default-include simply to make sure that the nameser_compat.h
dnl header *REALLY* can be include after the new nameser.h. It seems AIX 5.1
dnl (and others?) is not designed to allow this.
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

dnl *Sigh* these are needed in order for net/if.h to get properly detected.
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
]
  )

AC_CHECK_TYPE(socklen_t, ,
   AC_DEFINE(socklen_t, int, [the length of a socket address]), 
   [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
   ])

dnl check for AF_INET6
CARES_CHECK_CONSTANT(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif

  ], [PF_INET6], 
     AC_DEFINE_UNQUOTED(HAVE_PF_INET6,1,[Define to 1 if you have PF_INET6.])
)

dnl check for PF_INET6
CARES_CHECK_CONSTANT(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif

  ], [AF_INET6], 
     AC_DEFINE_UNQUOTED(HAVE_AF_INET6,1,[Define to 1 if you have AF_INET6.])
)


dnl check for the in6_addr structure
CARES_CHECK_STRUCT(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
  ], [in6_addr], 
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_IN6_ADDR,1,[Define to 1 if you have struct in6_addr.])
)

dnl check for the sockaddr_in6 structure
CARES_CHECK_STRUCT(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
  ], [sockaddr_in6], 
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_SOCKADDR_IN6,1,
       [Define to 1 if you have struct sockaddr_in6.]) ac_have_sockaddr_in6=yes
)

if test "$ac_have_sockaddr_in6" = "yes" ; then
CARES_CHECK_STRUCT_MEMBER(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
  ], [sockaddr_in6], [sin6_scope_id],
     AC_DEFINE_UNQUOTED(HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID,1,
       [Define to 1 if your struct sockaddr_in6 has sin6_scope_id.])
)
fi

dnl check for the addrinfo structure
CARES_CHECK_STRUCT(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
  ], [addrinfo], 
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_ADDRINFO,1,
       [Define to 1 if you have struct addrinfo.])
)

dnl check for inet_pton
AC_CHECK_FUNCS(inet_pton)
dnl Some systems have it, but not IPv6
if test "$ac_cv_func_inet_pton" = "yes" ; then
AC_MSG_CHECKING(if inet_pton supports IPv6)
AC_TRY_RUN(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
int main()
  {
    struct in6_addr addr6;
    if (inet_pton(AF_INET6, "::1", &addr6) < 1)
      exit(1);
    else
      exit(0);
  }
  ], [
       AC_MSG_RESULT(yes)
       AC_DEFINE_UNQUOTED(HAVE_INET_PTON_IPV6,1,[Define to 1 if inet_pton supports IPv6.])
     ], AC_MSG_RESULT(no),AC_MSG_RESULT(no))
fi
dnl Check for inet_net_pton
AC_CHECK_FUNCS(inet_net_pton)
dnl Again, some systems have it, but not IPv6
if test "$ac_cv_func_inet_net_pton" = "yes" ; then
AC_MSG_CHECKING(if inet_net_pton supports IPv6)
AC_TRY_RUN(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
int main()
  {
    struct in6_addr addr6;
    if (inet_net_pton(AF_INET6, "::1", &addr6, sizeof(addr6)) < 1)
      exit(1);
    else
      exit(0);
  }
  ], [
       AC_MSG_RESULT(yes)
       AC_DEFINE_UNQUOTED(HAVE_INET_NET_PTON_IPV6,1,[Define to 1 if inet_net_pton supports IPv6.])
     ], AC_MSG_RESULT(no),AC_MSG_RESULT(no))
fi


dnl Check for inet_ntop
AC_CHECK_FUNCS(inet_ntop)
dnl Again, some systems have it, but not IPv6
if test "$ac_cv_func_inet_ntop" = "yes" ; then
AC_MSG_CHECKING(if inet_ntop supports IPv6)
AC_TRY_RUN(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <errno.h>
int main()
  {
    struct in6_addr addr6;
    char buf[128];
    if (inet_ntop(AF_INET6, &addr6, buf, 128) == 0 && errno == EAFNOSUPPORT)
      exit(1);
    else
      exit(0);
  }
  ], [
       AC_MSG_RESULT(yes)
       AC_DEFINE_UNQUOTED(HAVE_INET_NTOP_IPV6,1,[Define to 1 if inet_ntop supports IPv6.])
     ], AC_MSG_RESULT(no),AC_MSG_RESULT(no))
fi

AC_CHECK_SIZEOF(struct in6_addr, ,
[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
]
)
AC_CHECK_SIZEOF(struct in_addr, ,
[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
]
)

AC_CHECK_FUNCS([bitncmp if_indextoname])


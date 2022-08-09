dnl $Id: config.m4 274405 2009-01-23 17:53:42Z tony2001 $

PHP_ARG_ENABLE(funcmap, whether to enable funcmap support,
[  --enable-funcmap           Enable funcmap support])

if test "$PHP_FUNCMAP" != "no"; then
  orig_LIBS="$LIBS"
  LIBS="$LIBS -pthread"
  AC_MSG_CHECKING([for pthread_atfork()])
  AC_TRY_RUN(
  [
#include <pthread.h>
#include <stdio.h>
void pthread_atfork_test(void)
{

}

main() {

  if(pthread_atfork(NULL, NULL, pthread_atfork_test)) {
    puts("pthread_atfork() failed.");
    return -1;
  }
  return 0;
}
  ],
  [ dnl -Success-
    AC_MSG_RESULT([ok])
    PHP_ADD_LIBRARY(pthread, 1, FUNCMAP_SHARED_LIBADD)
    PHP_LDFLAGS="$PHP_LDFLAGS -pthread"
  ],
  [ dnl -Failure-
    AC_MSG_ERROR([Pthreads are broken in your system?])
  ],
  [ dnl -Cross-compiling-
    dnl *fingers crossed*
    AC_MSG_RESULT([ok])
    PHP_ADD_LIBRARY(pthread, 1, FUNCMAP_SHARED_LIBADD)
    PHP_LDFLAGS="$PHP_LDFLAGS -pthread"
  ])
  LIBS="$orig_LIBS"
  PHP_SUBST(FUNCMAP_SHARED_LIBADD)
  PHP_NEW_EXTENSION(funcmap, funcmap.c, $ext_shared)
fi

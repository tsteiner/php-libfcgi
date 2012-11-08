PHP_ARG_ENABLE(libfcgi, "Enable libfcgi support", [ --enable-libfcgi   Enable libfcgi support])
if test "$PHP_LIBFCGI" = "yes"; then
  AC_DEFINE(HAVE_LIBFCGI, 1, [Whether you have libfcgi])
  PHP_NEW_EXTENSION(libfcgi, libfcgi.c, $ext_shared)
  PHP_ADD_LIBRARY(fcgi, 1, LIBFCGI_SHARED_LIBADD)
fi

PHP_SUBST(LIBFCGI_SHARED_LIBADD)


#ifndef PHP_LIBFCGI_H
#define PHP_LIBFCGI_H 1

#define PHP_LIBFCGI_VERSION "1.0"
#define PHP_LIBFCGI_EXTNAME "libfcgi"

PHP_FUNCTION(fcgi_is_cgi);
PHP_FUNCTION(fcgi_accept);
PHP_FUNCTION(fcgi_finish);

extern zend_module_entry libfcgi_module_entry;
#define phpext_fcgi_ptr &libfcgi_module_entry

#endif


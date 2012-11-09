#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_output.h"
#include "php_libfcgi.h"
#include "php_variables.h"
#include "sapi/cli/cli.h"
#include <fcgiapp.h>
#include "SAPI.h"
#include "ext/session/php_session.h"

static zend_function_entry libfcgi_functions[] = {
    PHP_FE(fcgi_is_cgi, NULL)
    PHP_FE(fcgi_accept, NULL)
    PHP_FE(fcgi_finish, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry libfcgi_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_LIBFCGI_EXTNAME,
    libfcgi_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_LIBFCGI_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_LIBFCGI
ZEND_GET_MODULE(libfcgi)
#endif

FCGX_Request request;

int libfcgi_write(const char *str, uint str_length TSRMLS_DC)
{
    return FCGX_PutS(str, request.out);
}

void libfcgi_flush(void *server_context)
{
    FCGX_FFlush(request.out);
}

int libfcgi_send_headers(sapi_headers_struct *sapi_headers TSRMLS_DC)
{
    zend_llist_position pos;
    sapi_header_struct *header;
    
    if (SG(request_info).no_headers == 1) {
        return SAPI_HEADER_SENT_SUCCESSFULLY;
    }
    
    if (sapi_headers->http_status_line || sapi_headers->http_response_code) {
        zend_bool has_status = 0;
        char *s, buf[256];
        int len;
        
        if (sapi_headers->http_status_line &&
            (s = strchr(sapi_headers->http_status_line, ' ')) != 0 &&
            (s - sapi_headers->http_status_line) >= 5 &&
            strncasecmp(sapi_headers->http_status_line, "HTTP/", 5) == 0
        ) {
            len = slprintf(buf, sizeof(buf), "Status:%s\r\n", s);
            has_status = 1;
        }
        if (!has_status && sapi_headers->http_response_code) {
            len = slprintf(buf, sizeof(buf), "Status: %d\r\n", sapi_headers->http_response_code);
        }
        
        PHPWRITE_H(buf, len);
    }
    
    for (
        header = zend_llist_get_first_ex(&sapi_headers->headers, &pos);
        header;
        header = zend_llist_get_next_ex(&sapi_headers->headers, &pos)
    ) {
        if (!header->header_len) {
            continue;
        }
        PHPWRITE_H(header->header, header->header_len);
        PHPWRITE_H("\r\n", 2);
    }
    PHPWRITE_H("\r\n", 2);
    
    return SAPI_HEADER_SENT_SUCCESSFULLY;
}

#define ARRAY_UNSET_KEY(a, k) \
    add_assoc_null(a, k);     \
    zend_hash_del((a)->value.ht, k, sizeof(k));

void libfcgi_finish()
{
    php_output_flush_all();
    
    if (PS(session_status) == php_session_active) {
        // need to destroy the current session
    }
    
    SG(headers_sent) = 0;
}

PHP_FUNCTION(fcgi_is_cgi)
{
    RETURN_LONG(FCGX_IsCGI());
}

PHP_FUNCTION(fcgi_accept)
{
    static zend_bool fcgi_is_ready = 0;
    char **param;
    zval **server_vars;
    
    zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &server_vars);
    
    if (!fcgi_is_ready) {
        if (FCGX_Init()) {
            RETURN_FALSE;
        }

        if (FCGX_InitRequest(&request, 0, 0)) {
            RETURN_FALSE;
        }
        
        sapi_module.ub_write = libfcgi_write;
        sapi_module.flush = libfcgi_flush;
        sapi_module.header_handler = NULL;
        sapi_module.send_headers = libfcgi_send_headers;
        sapi_module.send_header = NULL;
        
        ARRAY_UNSET_KEY(*server_vars, "PHP_SELF");
        ARRAY_UNSET_KEY(*server_vars, "SCRIPT_NAME");
        ARRAY_UNSET_KEY(*server_vars, "SCRIPT_FILENAME");
        ARRAY_UNSET_KEY(*server_vars, "PATH_TRANSLATED");
        ARRAY_UNSET_KEY(*server_vars, "DOCUMENT_ROOT");
        ARRAY_UNSET_KEY(*server_vars, "REQUEST_TIME_FLOAT");
        ARRAY_UNSET_KEY(*server_vars, "REQUEST_TIME");
        ARRAY_UNSET_KEY(*server_vars, "argv");
        ARRAY_UNSET_KEY(*server_vars, "argc");
        
        fcgi_is_ready = 1;
    }
    
    libfcgi_finish();
    
    
    if (FCGX_Accept_r(&request)) {
        RETURN_FALSE;
    }
    
    for (param = request.envp; param && *param; param++) {
        char *name, *value;
        int offset = strcspn(*param, "=");
        name = *param;
        
        name[offset] = '\0';
        value = name + offset + 1;
        
        add_assoc_string(*server_vars, name, value, 1);
        
        if (strncmp(name, "QUERY_STRING", sizeof("QUERY_STRING")) == 0) {
            SG(request_info).query_string = value;
        }
    }
    
    sapi_module.treat_data(PARSE_GET, NULL, NULL TSRMLS_CC);
    zend_hash_update(&EG(symbol_table), "_GET", sizeof("_GET"), &PG(http_globals)[TRACK_VARS_GET], sizeof(zval *), NULL);
    Z_ADDREF_P(PG(http_globals)[TRACK_VARS_GET]);

    RETURN_TRUE;
}

PHP_FUNCTION(fcgi_finish)
{
    libfcgi_finish();
    FCGX_Finish_r(&request);
}

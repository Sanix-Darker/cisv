/*
 * CISV PHP Extension
 * High-performance CSV parser with SIMD optimizations
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"

#include "cisv/parser.h"
#include "cisv/writer.h"
#include "cisv/transformer.h"

/* Extension information */
#define PHP_CISV_VERSION "0.0.8"
#define PHP_CISV_EXTNAME "cisv"

/* Parser object structure */
typedef struct {
    cisv_parser *parser;
    cisv_config config;
    cisv_transform_pipeline_t *pipeline;  /* Transform pipeline (may be NULL if unused) */
    zval rows;        /* Embedded zval - safer memory management */
    zval current_row; /* Embedded zval - safer memory management */
    zend_object std;
} cisv_parser_object;

static zend_class_entry *cisv_parser_ce;
static zend_object_handlers cisv_parser_handlers;

/* Get object from zend_object */
static inline cisv_parser_object *cisv_parser_from_obj(zend_object *obj) {
    return (cisv_parser_object *)((char *)obj - XtOffsetOf(cisv_parser_object, std));
}

#define Z_CISV_PARSER_P(zv) cisv_parser_from_obj(Z_OBJ_P(zv))

/* Callbacks */
static void php_cisv_field_callback(void *user, const char *data, size_t len) {
    cisv_parser_object *obj = (cisv_parser_object *)user;
    zval field;
    ZVAL_STRINGL(&field, data, len);
    add_next_index_zval(&obj->current_row, &field);
}

static void php_cisv_row_callback(void *user) {
    cisv_parser_object *obj = (cisv_parser_object *)user;

    /* Add current row to rows array */
    zval row_copy;
    ZVAL_COPY(&row_copy, &obj->current_row);
    add_next_index_zval(&obj->rows, &row_copy);

    /* Clear current row for next - properly reinitialize the zval */
    /* First destroy the old array data */
    zval_ptr_dtor(&obj->current_row);
    /* Then reinitialize the zval structure before calling array_init */
    ZVAL_UNDEF(&obj->current_row);
    array_init(&obj->current_row);
}

/* Create object */
static zend_object *cisv_parser_create_object(zend_class_entry *ce) {
    cisv_parser_object *intern = ecalloc(1, sizeof(cisv_parser_object) + zend_object_properties_size(ce));

    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);
    intern->std.handlers = &cisv_parser_handlers;

    /* Initialize config */
    cisv_config_init(&intern->config);

    /* Initialize pipeline to NULL (created lazily if transforms are used) */
    intern->pipeline = NULL;

    /* Initialize embedded zvals - no separate allocation needed */
    array_init(&intern->rows);
    array_init(&intern->current_row);

    return &intern->std;
}

/* Free object */
static void cisv_parser_free_object(zend_object *obj) {
    cisv_parser_object *intern = cisv_parser_from_obj(obj);

    if (intern->parser) {
        cisv_parser_destroy(intern->parser);
        intern->parser = NULL;
    }

    /* SECURITY FIX: Free transform pipeline to prevent memory leak */
    if (intern->pipeline) {
        cisv_transform_pipeline_destroy(intern->pipeline);
        intern->pipeline = NULL;
    }

    /* Destroy embedded zvals - no efree needed since they're embedded */
    zval_ptr_dtor(&intern->rows);
    zval_ptr_dtor(&intern->current_row);

    zend_object_std_dtor(&intern->std);
}

/* PHP_METHOD(CisvParser, __construct) */
PHP_METHOD(CisvParser, __construct) {
    zval *options = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(options)
    ZEND_PARSE_PARAMETERS_END();

    cisv_parser_object *intern = Z_CISV_PARSER_P(ZEND_THIS);

    /* Apply options if provided */
    if (options) {
        zval *val;

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "delimiter", sizeof("delimiter") - 1)) != NULL) {
            if (Z_TYPE_P(val) == IS_STRING) {
                /* SECURITY: Validate delimiter - must be exactly 1 character */
                if (Z_STRLEN_P(val) != 1) {
                    zend_throw_exception(zend_ce_exception, "Delimiter must be exactly 1 character", 0);
                    return;
                }
                char delim = Z_STRVAL_P(val)[0];
                /* SECURITY: Reject invalid delimiter characters */
                if (delim == '\0' || delim == '\n' || delim == '\r') {
                    zend_throw_exception(zend_ce_exception, "Invalid delimiter character (NUL, newline, or carriage return not allowed)", 0);
                    return;
                }
                intern->config.delimiter = delim;
            }
        }

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "quote", sizeof("quote") - 1)) != NULL) {
            if (Z_TYPE_P(val) == IS_STRING && Z_STRLEN_P(val) > 0) {
                intern->config.quote = Z_STRVAL_P(val)[0];
            }
        }

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "trim", sizeof("trim") - 1)) != NULL) {
            intern->config.trim = zend_is_true(val);
        }

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "skip_empty", sizeof("skip_empty") - 1)) != NULL) {
            intern->config.skip_empty_lines = zend_is_true(val);
        }
    }

    /* Set callbacks */
    intern->config.field_cb = php_cisv_field_callback;
    intern->config.row_cb = php_cisv_row_callback;
    intern->config.user = intern;

    /* Create parser */
    intern->parser = cisv_parser_create_with_config(&intern->config);
    if (!intern->parser) {
        zend_throw_exception(zend_ce_exception, "Failed to create parser", 0);
        return;
    }
}

/* PHP_METHOD(CisvParser, parseFile) */
PHP_METHOD(CisvParser, parseFile) {
    char *filename;
    size_t filename_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(filename, filename_len)
    ZEND_PARSE_PARAMETERS_END();

    cisv_parser_object *intern = Z_CISV_PARSER_P(ZEND_THIS);

    /* Clear previous data - properly reinitialize embedded zvals */
    zval_ptr_dtor(&intern->rows);
    ZVAL_UNDEF(&intern->rows);
    array_init(&intern->rows);
    zval_ptr_dtor(&intern->current_row);
    ZVAL_UNDEF(&intern->current_row);
    array_init(&intern->current_row);

    /* Parse file */
    int result = cisv_parser_parse_file(intern->parser, filename);
    if (result < 0) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Failed to parse file: %s", strerror(-result));
        return;
    }

    /* Return rows */
    RETURN_ZVAL(&intern->rows, 1, 0);
}

/* PHP_METHOD(CisvParser, parseString) */
PHP_METHOD(CisvParser, parseString) {
    char *csv;
    size_t csv_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(csv, csv_len)
    ZEND_PARSE_PARAMETERS_END();

    cisv_parser_object *intern = Z_CISV_PARSER_P(ZEND_THIS);

    /* Clear previous data - properly reinitialize embedded zvals */
    zval_ptr_dtor(&intern->rows);
    ZVAL_UNDEF(&intern->rows);
    array_init(&intern->rows);
    zval_ptr_dtor(&intern->current_row);
    ZVAL_UNDEF(&intern->current_row);
    array_init(&intern->current_row);

    /* Parse string */
    cisv_parser_write(intern->parser, (const uint8_t *)csv, csv_len);
    cisv_parser_end(intern->parser);

    /* Return rows */
    RETURN_ZVAL(&intern->rows, 1, 0);
}

/* PHP_METHOD(CisvParser, countRows) */
PHP_METHOD(CisvParser, countRows) {
    char *filename;
    size_t filename_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(filename, filename_len)
    ZEND_PARSE_PARAMETERS_END();

    size_t count = cisv_parser_count_rows(filename);
    RETURN_LONG((zend_long)count);
}

/* PHP_METHOD(CisvParser, setDelimiter) */
PHP_METHOD(CisvParser, setDelimiter) {
    char *delimiter;
    size_t delimiter_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(delimiter, delimiter_len)
    ZEND_PARSE_PARAMETERS_END();

    /* SECURITY: Validate delimiter - must be exactly 1 character */
    if (delimiter_len != 1) {
        zend_throw_exception(zend_ce_exception, "Delimiter must be exactly 1 character", 0);
        return;
    }

    /* SECURITY: Reject invalid delimiter characters */
    if (delimiter[0] == '\0' || delimiter[0] == '\n' || delimiter[0] == '\r') {
        zend_throw_exception(zend_ce_exception, "Invalid delimiter character (NUL, newline, or carriage return not allowed)", 0);
        return;
    }

    cisv_parser_object *intern = Z_CISV_PARSER_P(ZEND_THIS);
    intern->config.delimiter = delimiter[0];

    /* Recreate parser with new config */
    if (intern->parser) {
        cisv_parser_destroy(intern->parser);
    }
    intern->parser = cisv_parser_create_with_config(&intern->config);

    RETURN_ZVAL(ZEND_THIS, 1, 0);
}

/* PHP_METHOD(CisvParser, setQuote) */
PHP_METHOD(CisvParser, setQuote) {
    char *quote;
    size_t quote_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(quote, quote_len)
    ZEND_PARSE_PARAMETERS_END();

    if (quote_len == 0) {
        zend_throw_exception(zend_ce_exception, "Quote character cannot be empty", 0);
        return;
    }

    cisv_parser_object *intern = Z_CISV_PARSER_P(ZEND_THIS);
    intern->config.quote = quote[0];

    /* Recreate parser with new config */
    if (intern->parser) {
        cisv_parser_destroy(intern->parser);
    }
    intern->parser = cisv_parser_create_with_config(&intern->config);

    RETURN_ZVAL(ZEND_THIS, 1, 0);
}

/* Method table */
static const zend_function_entry cisv_parser_methods[] = {
    PHP_ME(CisvParser, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(CisvParser, parseFile, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(CisvParser, parseString, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(CisvParser, countRows, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(CisvParser, setDelimiter, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(CisvParser, setQuote, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* Module initialization */
PHP_MINIT_FUNCTION(cisv) {
    zend_class_entry ce;

    INIT_CLASS_ENTRY(ce, "CisvParser", cisv_parser_methods);
    cisv_parser_ce = zend_register_internal_class(&ce);
    cisv_parser_ce->create_object = cisv_parser_create_object;

    memcpy(&cisv_parser_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    cisv_parser_handlers.offset = XtOffsetOf(cisv_parser_object, std);
    cisv_parser_handlers.free_obj = cisv_parser_free_object;

    return SUCCESS;
}

/* Module info */
PHP_MINFO_FUNCTION(cisv) {
    php_info_print_table_start();
    php_info_print_table_header(2, "cisv support", "enabled");
    php_info_print_table_row(2, "Version", PHP_CISV_VERSION);
    php_info_print_table_row(2, "Features", "SIMD-accelerated parsing, zero-copy mmap");
    php_info_print_table_end();
}

/* Module entry */
zend_module_entry cisv_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_CISV_EXTNAME,
    NULL,
    PHP_MINIT(cisv),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(cisv),
    PHP_CISV_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_CISV
ZEND_GET_MODULE(cisv)
#endif

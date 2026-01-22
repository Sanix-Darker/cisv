/*
 * CISV PHP Extension Header
 * High-performance CSV parser with SIMD optimizations
 *
 * PIE-compatible: Bundled with core C library for single-step installation
 */

#ifndef PHP_CISV_H
#define PHP_CISV_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

/* Extension information */
#define PHP_CISV_VERSION "0.0.8"
#define PHP_CISV_EXTNAME "cisv"

/* Module entry point */
extern zend_module_entry cisv_module_entry;
#define phpext_cisv_ptr &cisv_module_entry

/* Module functions */
PHP_MINIT_FUNCTION(cisv);
PHP_MINFO_FUNCTION(cisv);

/* Thread safety */
#ifdef ZTS
#include "TSRM.h"
#endif

#endif /* PHP_CISV_H */

dnl config.m4 for extension cisv
PHP_ARG_ENABLE(cisv, whether to enable cisv support,
[  --enable-cisv           Enable cisv support])

if test "$PHP_CISV" != "no"; then
  dnl Add include path for core library headers
  PHP_ADD_INCLUDE([$srcdir/../../core/include])

  dnl Link against the pre-built libcisv library
  PHP_ADD_LIBRARY_WITH_PATH(cisv, $srcdir/../../core/build, CISV_SHARED_LIBADD)
  PHP_SUBST(CISV_SHARED_LIBADD)

  dnl Compiler flags for optimization
  CFLAGS="$CFLAGS -O3"

  dnl Only compile the PHP wrapper, link against libcisv
  PHP_NEW_EXTENSION(cisv, src/cisv_php.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi

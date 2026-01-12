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
  dnl Match core library optimization flags for maximum performance
  CFLAGS="$CFLAGS -O3 -march=native -mtune=native -ffast-math -funroll-loops -fomit-frame-pointer"

  dnl Enable Link-Time Optimization if supported
  AC_MSG_CHECKING([whether $CC supports -flto])
  _save_cflags="$CFLAGS"
  CFLAGS="$CFLAGS -flto"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
    [AC_MSG_RESULT([yes])
     LDFLAGS="$LDFLAGS -flto"],
    [AC_MSG_RESULT([no])
     CFLAGS="$_save_cflags"])

  dnl Only compile the PHP wrapper, link against libcisv
  PHP_NEW_EXTENSION(cisv, src/cisv_php.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi

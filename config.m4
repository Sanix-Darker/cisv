dnl config.m4 for extension cisv
dnl PIE-compatible: Bundles core C library with PHP extension

PHP_ARG_ENABLE(cisv, whether to enable cisv support,
[  --enable-cisv           Enable CISV high-performance CSV parser support])

if test "$PHP_CISV" != "no"; then
  dnl Add include path for core library headers
  PHP_ADD_INCLUDE([$srcdir/core/include])

  dnl Platform-specific settings
  case $host_os in
    linux*)
      CFLAGS="$CFLAGS -D_GNU_SOURCE"
      ;;
    darwin*)
      CFLAGS="$CFLAGS -D__APPLE__"
      ;;
  esac

  dnl Compiler flags for optimization
  dnl Match core library optimization flags for maximum performance
  CFLAGS="$CFLAGS -O3 -ffast-math -funroll-loops -fomit-frame-pointer"

  dnl Try to enable native CPU optimizations (SIMD: AVX2/AVX-512/NEON)
  AC_MSG_CHECKING([whether $CC supports -march=native])
  _save_cflags="$CFLAGS"
  CFLAGS="$CFLAGS -march=native"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
    [AC_MSG_RESULT([yes])
     CFLAGS="$CFLAGS -mtune=native"],
    [AC_MSG_RESULT([no])
     CFLAGS="$_save_cflags"])

  dnl Enable Link-Time Optimization if supported
  AC_MSG_CHECKING([whether $CC supports -flto])
  _save_cflags="$CFLAGS"
  CFLAGS="$CFLAGS -flto"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
    [AC_MSG_RESULT([yes])
     LDFLAGS="$LDFLAGS -flto"],
    [AC_MSG_RESULT([no])
     CFLAGS="$_save_cflags"])

  dnl Check for pthread (required for parallel parsing)
  AC_CHECK_LIB(pthread, pthread_create, [
    PHP_ADD_LIBRARY(pthread, 1, CISV_SHARED_LIBADD)
  ])
  PHP_SUBST(CISV_SHARED_LIBADD)

  dnl Bundle core C library sources with PHP extension
  dnl This eliminates the need for separate core build step
  PHP_NEW_EXTENSION(cisv,
    cisv.c \
    core/src/parser.c \
    core/src/writer.c \
    core/src/transformer.c,
    $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)

  dnl Add core subdirectory for out-of-tree builds
  PHP_ADD_BUILD_DIR([$ext_builddir/core/src])
fi

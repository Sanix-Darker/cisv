dnl config.m4 for extension cisv
PHP_ARG_ENABLE(cisv, whether to enable cisv support,
[  --enable-cisv           Enable cisv support])

if test "$PHP_CISV" != "no"; then
  dnl Add include path for core library
  PHP_ADD_INCLUDE([$srcdir/../../core/include])

  dnl Compiler flags for SIMD optimization
  CFLAGS="$CFLAGS -O3 -march=native -mavx2 -mtune=native -ffast-math"

  dnl Define sources
  PHP_NEW_EXTENSION(cisv, [
    src/cisv_php.c \
    ../../core/src/parser.c \
    ../../core/src/writer.c \
    ../../core/src/transformer.c
  ], $ext_shared)
fi

PHP_ARG_ENABLE(cisv, whether to enable cisv support,
[  --enable-cisv           Enable cisv support])

if test "$PHP_CISV" != "no"; then
  # Detect SIMD support
  case $host_cpu in
    x86_64|amd64)
      EXTRA_CFLAGS="$EXTRA_CFLAGS -mavx2 -march=native"
      ;;
    arm*|aarch64)
      EXTRA_CFLAGS="$EXTRA_CFLAGS -mfpu=neon"
      ;;
  esac

  PHP_ADD_INCLUDE(../../include)
  PHP_NEW_EXTENSION(cisv, cisv_php.c, $ext_shared,, $EXTRA_CFLAGS)
fi

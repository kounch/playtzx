dnl Process this file with autoconf to produce a configure script.

AC_INIT(playtzx, 0.12b)
AC_CONFIG_SRCDIR(playtzx.c)
AM_CONFIG_HEADER(config.h)

dnl Use automake to produce `Makefile.in'
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_CC

# Get flags
AC_CANONICAL_HOST
AC_MSG_CHECKING(for audio API)
case $host in
  *-*-linux*)
  # Look for OSS flag
  AC_ARG_WITH(oss, [  --with-oss = choose OSS API support (GNU/Linux only)], [
    api="$api -D__LINUX_OSS__"
    AC_MSG_RESULT(using OSS)], )

  # If no audio api flags specified, use OSS
  if [test "$api" == "";] then
    AC_MSG_RESULT(using OSS)
    AC_SUBST( api, [-D__LINUX_OSS__] )
  fi
  ;;

  *-apple*)
  # Look for Core flag
  AC_ARG_WITH(core, [  --with-core = choose CoreAudio API support (MAC OSX only)], [
    api="$api -D__MACOSX_CORE__"
    AC_MSG_RESULT(using CoreAudio)
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [], [AC_MSG_ERROR(CoreAudio header files not found!)] )
    LIBS="$LIBS -framework CoreAudio" ], )

  # If no audio api flags specified, use CoreAudio
  if [test "$api" == ""; ] then
    AC_SUBST( api, [-D__MACOSX_CORE__] )
    AC_MSG_RESULT(using CoreAudio)
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h,
      [],
      [AC_MSG_ERROR(CoreAudio header files not found!)] )
    LIBS="$LIBS -framework CoreAudio"
  fi

  ;;
esac

dnl Do we want lots of warning messages?
AC_MSG_CHECKING(whether lots of warnings requested)
AC_ARG_ENABLE(warnings,
[  --enable-warnings       give lots of warnings if using gcc],
if test "$enableval" = yes; then
    warnings=yes;
else
    warnings=no;
fi,
warnings=no)
AC_MSG_RESULT($warnings)

dnl If it appears we're using gcc as our compiler, turn on warnings
if test "$ac_cv_prog_cc_g" = yes; then
    CFLAGS="$CFLAGS -Wall"
    dnl And possibly lots of warnings
    if test "$warnings" = yes; then
        CFLAGS="$CFLAGS -Wstrict-prototypes -Wmissing-prototypes -Winline -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Waggregate-return -W -Wsign-compare"
    fi
fi

AC_OUTPUT(Makefile)

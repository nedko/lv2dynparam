# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([lv2dynparam1],[2])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([subdir-objects])
LT_INIT

AC_CONFIG_SRCDIR([lv2dynparam.h])
AC_CONFIG_HEADERS([config.h])

if test -d $srcdir/.git ; then dev_version="yes" ; else dev_version="no"; fi
AM_CONDITIONAL(DEV_VERSION, test "$dev_version" = "yes")

# Checks for programs.
AC_PROG_CC

#DX_HTML_FEATURE(ON)
#DX_CHM_FEATURE(OFF)
#DX_CHI_FEATURE(OFF)
#DX_MAN_FEATURE(OFF)
#DX_RTF_FEATURE(OFF)
#DX_XML_FEATURE(OFF)
#DX_PDF_FEATURE(OFF)
#DX_PS_FEATURE(OFF)
#
#DX_INIT_DOXYGEN(lv2dynparam, doxygen.cfg)

# Checks for libraries.

PKG_CHECK_MODULES(LV2, lv2core >= 1)

# Checks for typedefs, structures, and compiler characteristics.
#AC_C_CONST
#AC_C_INLINE
#AC_TYPE_SIZE_T
#AC_TYPE_UINT32_T

# Checks for library functions.
#AC_FUNC_MALLOC
#AC_FUNC_VPRINTF

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([plugin/Makefile])
AC_CONFIG_FILES([host/Makefile])
AC_CONFIG_FILES([lv2dynparamplugin1.pc])
AC_CONFIG_FILES([lv2dynparamhost1.pc])
AC_OUTPUT

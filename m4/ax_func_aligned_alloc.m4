# AX_FUNC_ALIGNED_ALLOC
# ---------------------
#
# Check if a specified machine type cannot handle misaligned data. That is, multi-byte data
# types which are not properly aligned in memory fail. Many machines are happy to work with
# misaligned data, but slowing down a bit. Other machines just won't tolerate such data.
#
# This is a simple lookup amongst machines known to the current autotools. So far we only deal
# with the ARM and sparc.
# A lookup is used, as many of the devices which cannot handled misaligned access are embedded
# processors, for which the code normally be cross-compiled. 
#
AC_DEFUN([AX_FUNC_ALIGNED_ALLOC],[
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Werror"
    AC_CACHE_CHECK([checking for aligned_alloc],
                   [ax_cv_func_aligned_alloc],
                   [AC_LINK_IFELSE([AC_LANG_PROGRAM([
			    									 #define _ISOC11_SOURCE
                                                     #include <stdlib.h>
                                                    ],
                                                    [
                                                       aligned_alloc(0,0);
                                                    ])],
                                                    [ax_cv_func_aligned_alloc=yes],
                                                    [ax_cv_func_aligned_alloc=no])])

  if test "x${ax_cv_func_aligned_alloc}" = "xyes" ; then
    AC_DEFINE([HAVE_ALIGNED_ALLOC], [1], [Define to 1 if you have the aligned_alloc() function.])
  fi
CFLAGS="$saved_CFLAGS"
])# AX_ALIGNED_ALLOC

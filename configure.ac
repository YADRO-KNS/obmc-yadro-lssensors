AC_PREREQ([2.69])
AC_INIT([phosphor-list-sensors], [1.0], [https://b.yadro.com/projects/SRV/repos/phosphor-list-sensors])
AC_LANG([C++])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign])
AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CXX
AC_PROG_MAKE_SET

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext])
AX_APPEND_COMPILE_FLAGS([-fpic -Wall -Werror], [CXXFLAGS])

# Checks for libraries.
AX_PKG_CHECK_MODULES([SYSTEMD], [], [libsystemd >= 221], [], [AC_MSG_ERROR(["systemd required and not found."])])

# sdbusplus is non-private because we have no library to link.
# If we ever have a library, move this to private.
AX_PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus], [], [], [AC_MSG_ERROR(["sdbusplus required and not found."])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT


AC_PREREQ([2.69])
AC_INIT([phosphor-list-sensors], [1.0], [https://b.yadro.com/projects/SRV/repos/phosphor-list-sensors])
AC_LANG([C++])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign])
AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CXX
AC_PROG_MAKE_SET

# Checks for libraries.
PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus], [], [], [AC_MSG_ERROR(["Requires sdbusplus package."])])

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_17([noext])
AX_APPEND_COMPILE_FLAGS([-fpic -Wall -Werror], [CXXFLAGS])

# Checks for library functions.
LT_INIT
 
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

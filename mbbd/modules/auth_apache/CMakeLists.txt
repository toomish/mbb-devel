pkg_check_modules (APR_UTIL apr-util-1)
if (NOT APR_UTIL_FOUND)
	message (STATUS "apr-util not found, module auth_apache will not be build")
	return ()
endif()

file (GLOB auth_apache_sources *.c)

include_directories (${MBB_MODULES_DIR} ${APR_UTIL_INCLUDE_DIRS})
add_definitions (${APR_UTIL_CFLAGS_OTHER})
link_directories (${APR_UTIL_LIBRARY_DIRS})

mbb_define_module (auth_apache "${auth_apache_sources}" "${APR_UTIL_LIBRARIES}")

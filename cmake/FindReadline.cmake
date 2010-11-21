find_path (READLINE_INCLUDE_DIR readline/readline.h)
find_library (READLINE_LIBRARY readline)

if (READLINE_LIBRARY)
	find_library (CURSES_LIBRARY NAMES ncursesw ncurses cursesw curses)
	if (NOT CURSES_LIBRARY)
		set (READLINE_LIBRARIES ${READLINE_LIBRARY})
	else()
		set (READLINE_LIBRARIES ${READLINE_LIBRARY} ${CURSES_LIBRARY})
	endif()

	include (CheckLibraryExists)

	set (CMAKE_REQUIRED_LIBRARIES ${READLINE_LIBRARIES})
	check_library_exists (readline readline "" HAVE_READLINE)
	if (HAVE_READLINE)
		message (STATUS "trying to compile readline - ok")
	else()
		message (STATUS "compile readline failed")
		unset (READLINE_LIBRARY CACHE)
	endif()

	unset (CMAKE_REQUIRED_LIBRARIES CACHE)
endif()

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args (Readline DEFAULT_MSG
	READLINE_LIBRARY READLINE_LIBRARIES READLINE_INCLUDE_DIR)
mark_as_advanced (READLINE_LIBRARY READLINE_LIBRARIES READLINE_INCLUDE_DIR)

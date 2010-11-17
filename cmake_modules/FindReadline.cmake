find_path (READLINE_INCLUDE_DIR readline/readline.h)
find_library (READLINE_LIBRARY readline)

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args (Readline DEFAULT_MSG READLINE_LIBRARY READLINE_INCLUDE_DIR)
mark_as_advanced (READLINE_LIBRARY READLINE_INCLUDE_DIR)

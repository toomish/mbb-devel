set (MBBD_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory (modules)

find_program (PG_CONFIG NAMES pg_config DOC "pg_config executable")
if (NOT PG_CONFIG)
	message (FATAL_ERROR "pg_config not found, you don't seem to have PostgreSQL libraries installed...")
endif()

macro (_pg_config _opt _var)
	execute_process (
		COMMAND ${PG_CONFIG} --${_opt}
		OUTPUT_VARIABLE _pg_config_result
		RESULT_VARIABLE _pg_config_failed)

	if (_pg_config_failed)
		message (FATAL_ERROR "pg_config failed")
	else()
		string (REGEX REPLACE "[\r\n]"	" " _pg_config_result "${_pg_config_result}")
		string (REGEX REPLACE " +$"	""  _pg_config_result "${_pg_config_result}")
		set (${_var} ${_pg_config_result})
	endif()
endmacro (_pg_config)

_pg_config(version PGSQL_VERSION_STRING)
string (REGEX REPLACE " " ";" PGSQL_VERSION_STRING "${PGSQL_VERSION_STRING}")
list(GET PGSQL_VERSION_STRING 1 PGSQL_VERSION)
set (PGSQL_VERSION_STRING)

message (STATUS "  found PostgreSQL, version ${PGSQL_VERSION}")

_pg_config(includedir PGSQL_INCLUDE_DIRS)
_pg_config(libdir PGSQL_LIBRARY_DIRS)

include_directories (${PGSQL_INCLUDE_DIRS})
link_directories (${PGSQL_LIBRARY_DIRS})

file (GLOB mbbd_sources *.c)

add_executable (mbbd ${mbbd_sources})
target_link_libraries (mbbd mbbutil pq ${MBB_LIBRARIES})

install (TARGETS mbbd DESTINATION bin)

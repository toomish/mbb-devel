set (MBB_MODULES attr auth_apache netflow)

set (MBB_MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set (MBB_MODULES_INSTALL_DIR lib/mbb)

include_directories (${MBBD_SOURCE_DIR})

macro (mbb_define_module _name _sources)
	add_library (${_name} MODULE ${_sources})
	target_link_libraries (${_name} ${MBB_LIBRARIES})

	if (${ARGC} GREATER 2)
		target_link_libraries (${_name} ${ARGV2})
	endif ()

	set_target_properties (${_name} PROPERTIES PREFIX "")
	install (TARGETS ${_name} DESTINATION ${MBB_MODULES_INSTALL_DIR})
endmacro (mbb_define_module)

foreach (_mod ${MBB_MODULES})
	add_subdirectory (${_mod})
endforeach ()

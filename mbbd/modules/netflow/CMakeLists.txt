find_library (FT_LIBRARY NAMES ft HINTS $ENV{FLOW_TOOLS_DIR})
if (NOT FT_LIBRARY)
	message (STATUS "flow-tools library not found")
	return ()
endif()

find_path (FLOW_TOOLS_INCLUDE_DIR NAMES ftlib.h HINTS $ENV{FLOW_TOOLS_DIR})
if (NOT FLOW_TOOLS_INCLUDE_DIR)
	message (STATUS "flow-tools header not found")
	return ()
endif()

get_filename_component (FLOW_TOOLS_LIBRARY_DIR ${FT_LIBRARY} PATH)
set (FLOW_TOOLS_LIBRARIES -lft -lz)

file (GLOB netflow_sources *.c)

include_directories (${MBB_MODULES_DIR})
link_directories (${FLOW_TOOLS_LIBRARY_DIR})

set_source_files_properties (flow.c
	COMPILE_FLAGS "-D_BSD_SOURCE -I${FLOW_TOOLS_INCLUDE_DIR}"
)

mbb_define_module (netflow "${netflow_sources}" "${FLOW_TOOLS_LIBRARIES}")

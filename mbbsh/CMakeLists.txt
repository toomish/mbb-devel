find_package (Lua51)
if (NOT LUA51_FOUND)
	message (FATAL_ERROR "lua51 not found")
endif ()

find_package (Readline)
if (NOT READLINE_FOUND)
	message (FATAL_ERROR "readline not found")
endif ()

set (MBBSH_LUA_DIR share/mbb/lua)

include_directories (${LUA_INCLUDE_DIR} ${READLINE_INCLUDE_DIR})

file (GLOB mbbsh_sources *.c)

add_definitions (-DDEFAULT_LUA_DIR=\"${CMAKE_INSTALL_PREFIX}/${MBBSH_LUA_DIR}\")
set_source_files_properties (luaenv.c COMPILE_FLAGS -D_BSD_SOURCE)
set_source_files_properties (nettalker.c COMPILE_FLAGS -D_GNU_SOURCE)

add_executable (mbbsh ${mbbsh_sources})
target_link_libraries (mbbsh mbbutil
	${MBB_LIBRARIES} ${LUA_LIBRARIES} ${READLINE_LIBRARIES})

install (TARGETS mbbsh DESTINATION bin)
install (DIRECTORY lua DESTINATION share/mbb FILES_MATCHING PATTERN "*.lua")

find_package (Lua51)
if (NOT LUA51_FOUND)
	message (FATAL_ERROR "lua51 not found")
endif ()

find_package (Readline)
if (NOT READLINE_FOUND)
	message (FATAL_ERROR "readline not found")
endif ()

include_directories (${LUA_INCLUDE_DIR} ${READLINE_INCLUDE_DIR})

file (GLOB mbbsh_sources *.c)

add_executable (mbbsh ${mbbsh_sources})
target_link_libraries (mbbsh mbbutil ${MBB_LIBRARIES} ${LUA_LIBRARIES} ${READLINE_LIBRARY})

install (TARGETS mbbsh DESTINATION bin)
install (DIRECTORY lua DESTINATION share/mbb)

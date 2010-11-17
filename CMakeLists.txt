cmake_minimum_required (VERSION 2.6)
project (mbb C)

set (CMAKE_BUILD_TYPE Debug)

include (FindPkgConfig)
set (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules/)

pkg_check_modules (GLIB REQUIRED glib-2.0>=2.24)
pkg_check_modules (GTHREAD REQUIRED gthread-2.0)

add_definitions (-DNOCOLOR -DNDEBUG)

include_directories (include ${GLIB_INCLUDE_DIRS} ${GTHREAD_INCLUDE_DIRS})
link_directories (${GLIB_LIBRARY_DIRS} ${GTHREAD_LIBRARY_DIRS})

set (MBB_LIBRARIES ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})

add_subdirectory (lib)
add_subdirectory (mbbd)
add_subdirectory (mbbsh)

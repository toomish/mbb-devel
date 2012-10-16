file (GLOB auth_salt_sources *.c)
include_directories (${MBB_MODULES_DIR})
mbb_define_module(auth_salt "${auth_salt_sources}")

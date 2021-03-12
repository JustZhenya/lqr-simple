cmake_minimum_required(VERSION 3.1)

project(lqr-simple LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(lqr-simple SHARED
	src/lqr_carver_bias.c
	src/lqr_carver_list.c
	src/lqr_carver_rigmask.c
	src/lqr_carver.c
	src/lqr_cursor.c
	src/lqr_energy.c
	src/lqr_gradient.c
	src/lqr_progress.c
	src/lqr_rwindow.c
	src/lqr_vmap_list.c
	src/lqr_vmap.c
)

include(GNUInstallDirs)

set(
	INST_HEADERS
	src/lqr.h
	src/lqr_base.h
	src/lqr_gradient_pub.h
	src/lqr_rwindow_pub.h
	src/lqr_energy_pub.h
	src/lqr_cursor_pub.h
	src/lqr_progress_pub.h
	src/lqr_vmap_pub.h
	src/lqr_vmap_list_pub.h
	src/lqr_carver_list_pub.h
	src/lqr_carver_bias_pub.h
	src/lqr_carver_rigmask_pub.h
	src/lqr_carver_pub.h
)

install(
	FILES ${INST_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
	TARGETS lqr-simple EXPORT lqr-simple-config
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
	EXPORT lqr-simple-config DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lqr-simple
)

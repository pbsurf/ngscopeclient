# Standalone ps6000d (scopehal-pico-bridge) socket server, built from the same bridge code as the pico transport.
# Off by default: ngscopeclient's pico transport runs the same bridge in process. Turn on with -DBUILD_PS6000D=ON
# (also needs the Pico SDK headers, see lib/picobridge).

option(BUILD_PS6000D "Build the standalone ps6000d socket server (needs the Pico SDK headers)" OFF)

if(NOT BUILD_PS6000D OR NOT TARGET picobridge)
	return()
endif()

add_executable(ps6000d
	main.cpp)

target_link_libraries(ps6000d
	picobridge
	xptools)

install(TARGETS ps6000d RUNTIME)

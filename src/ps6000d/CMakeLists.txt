# Standalone ps6000d (scopehal-pico-bridge) socket server, built from the same bridge code as the pico transport.
# Only built if lib/picobridge found the Pico SDK headers.

if(NOT TARGET picobridge)
	return()
endif()

add_executable(ps6000d
	main.cpp)

target_link_libraries(ps6000d
	picobridge
	xptools)

install(TARGETS ps6000d RUNTIME)

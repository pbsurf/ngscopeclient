# Built in Pico bridge (scopehal-pico-bridge running in process), for the pico transport and the standalone ps6000d.
#
# Only the Pico SDK headers are needed to build. The SDK libraries are loaded at run time if they're installed, so they
# don't have to be on the build machine, and ngscopeclient still runs without them.
# If the headers aren't found, the picobridge target isn't created.

set(PICO_SDK_INCLUDE_DIR "" CACHE PATH "Directory with the Pico SDK headers (ps6000aApi.h etc), if not in the default place")
set(PICO_FOUND TRUE)
set(PICO_INCLUDE_DIRS "")
foreach(api ps2000a ps3000a ps4000a ps5000a ps6000a psospa)
	find_path(PICO_${api}_INCLUDE_DIR ${api}Api.h
		HINTS
			${PICO_SDK_INCLUDE_DIR}
			"C:/Program Files/Pico Technology/SDK/inc"
			/opt/picoscope/include
			/Library/Frameworks/PicoSDK.framework/Headers
		PATH_SUFFIXES lib${api})
	if(PICO_${api}_INCLUDE_DIR)
		list(APPEND PICO_INCLUDE_DIRS ${PICO_${api}_INCLUDE_DIR})
	else()
		set(PICO_FOUND FALSE)
	endif()
endforeach()

if(NOT PICO_FOUND)
	message("-- Pico SDK headers not found (set PICO_SDK_INCLUDE_DIR), the pico transport will not be available. "
		"PicoScopes can still be used through scopehal-pico-bridge and the twinlan transport.")
	return()
endif()

list(REMOVE_DUPLICATES PICO_INCLUDE_DIRS)
message("-- Found Pico SDK headers: ${PICO_INCLUDE_DIRS}")

add_library(picobridge STATIC
	BridgeSCPIServer.cpp
	PicoApi.cpp
	PicoDataPipe.cpp
	PicoOpen.cpp
	PicoSCPIServer.cpp
	SCPIServer.cpp
	WaveformServerThread.cpp)

# Users include "picobridge/PicoSCPIServer.h"
target_include_directories(picobridge
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(picobridge
	SYSTEM PUBLIC
		${PICO_INCLUDE_DIRS})

target_compile_definitions(picobridge PUBLIC HAS_PICO)

target_link_libraries(picobridge
	PUBLIC
		log
		Threads::Threads
		${CMAKE_DL_LIBS})

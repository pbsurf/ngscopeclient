# libiio 0.26: the last release with the 0.x API, which is the only one scopehal supports.
# (libiio 1.0 changed the API and ABI; upstream master is 1.x.)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO analogdevicesinc/libiio
    REF v0.26
    SHA512 f2febe8223149602e9d34957fb04892ff1d7449abf2923d0428d0db43148445a0b5595eb6d00013687c73001685b6aaaa5aa098ff67f51ec1950200330481bba
    HEAD_REF libiio-v0
    PATCHES
        # libiio relies on dead-code elimination to drop calls into disabled backends;
        # MSVC does not do that at /Od, so the Debug DLL fails to link without this
        msvc-debug-dead-code.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        usb WITH_USB_BACKEND
)

# libiio finds libusb via pkg-config first (with a find_library fallback)
vcpkg_find_acquire_program(PKGCONFIG)

# DNS-SD (finding radios on the network, e.g. ip:pluto.local in a scan) uses the bundled mdns code on Windows, but
# avahi on Linux, which a portable build can't count on. Without it, ip: URIs still work (the system resolver
# handles .local names through nss-mdns where it's set up), and USB scanning is unaffected.
if(VCPKG_TARGET_IS_LINUX)
    set(DNS_SD OFF)
else()
    set(DNS_SD ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        "-DPKG_CONFIG_EXECUTABLE=${PKGCONFIG}"
        -DWITH_NETWORK_BACKEND=ON
        -DWITH_XML_BACKEND=ON
        "-DHAVE_DNS_SD=${DNS_SD}"
        -DWITH_SERIAL_BACKEND=OFF
        -DWITH_ZSTD=OFF
        # Linux-only server side and system integration, not needed by a client
        -DWITH_IIOD=OFF
        -DINSTALL_UDEV_RULE=OFF
        # iio_info etc. are not needed to build ngscopeclient
        -DWITH_TESTS=OFF
        -DWITH_EXAMPLES=OFF
        -DWITH_DOC=OFF
        -DWITH_MAN=OFF
        -DCPP_BINDINGS=OFF
        -DCSHARP_BINDINGS=OFF
        -DPYTHON_BINDINGS=OFF
        -DOSX_FRAMEWORK=OFF
        -DOSX_PACKAGE=OFF
        -DENABLE_PACKAGING=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON
    MAYBE_UNUSED_VARIABLES
        CMAKE_DISABLE_FIND_PACKAGE_Doxygen
        INSTALL_UDEV_RULE
        OSX_PACKAGE
        WITH_IIOD
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

# MSVC builds name the import library libiio.lib, but upstream's .pc file says -liio
if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
    foreach(pc IN ITEMS
            "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/libiio.pc"
            "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/libiio.pc")
        if(EXISTS "${pc}")
            vcpkg_replace_string("${pc}" "-liio" "-llibiio")
        endif()
    endforeach()
endif()

# Upstream only defines LIBIIO_STATIC while building libiio itself; consumers of a
# static build would otherwise see __declspec(dllimport) declarations
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/iio.h"
        "#   ifdef LIBIIO_STATIC"
        "#   if 1 /* vcpkg: static build */")
endif()

# A static libiio needs its dependencies at link time, but upstream's .pc file doesn't name them
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(IIO_PC_REQUIRES "libxml-2.0")
    if("usb" IN_LIST FEATURES)
        string(APPEND IIO_PC_REQUIRES " libusb-1.0")
    endif()
    foreach(pc IN ITEMS
            "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/libiio.pc"
            "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/libiio.pc")
        if(EXISTS "${pc}")
            file(READ "${pc}" PC_CONTENTS)
            string(REGEX REPLACE "\nRequires:[^\n]*" "\nRequires: ${IIO_PC_REQUIRES}" PC_CONTENTS "${PC_CONTENTS}")
            file(WRITE "${pc}" "${PC_CONTENTS}")
        endif()
    endforeach()
endif()

vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING.txt")

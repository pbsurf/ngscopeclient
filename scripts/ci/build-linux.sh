#!/bin/bash
# Portable Linux x64 build of ngscopeclient, as built by .github/workflows/build-linux.yml.
#
# Meant to run in quay.io/pypa/manylinux_2_28_x86_64 (AlmaLinux 8: glibc 2.28, gcc-toolset), with the dependencies
# from vcpkg (static, the same manifest as the Windows build, msvc/vcpkg.json), so the program runs on any
# distribution with glibc 2.28 or later. Only glibc, libstdc++/libgcc_s/libgomp, the Vulkan loader and the
# X11/Wayland client libraries, libudev and libdbus come from the user's system. scripts/ci/linux-container.sh runs it locally in the
# same container.
#
# Usage: scripts/ci/build-linux.sh [step...]
#   deps       install the build dependencies (dnf, needs root)
#   vcpkg      clone and bootstrap vcpkg at the manifest's baseline, if VCPKG_ROOT isn't a vcpkg checkout yet
#   configure  configure (vcpkg builds the dependencies on the first run, which takes a while)
#   build      build
#   test       run the unit tests, on Mesa's software Vulkan driver (lavapipe) if it's installed: the test
#              executables in $TEST_DIR/tests, the build tree or an extracted test bundle (see package). Not one
#              of the default steps: EL8's lavapipe fails to create an instance, so CI runs this on a newer
#              distro, not in the container.
#   package    make $DIST_DIR/ngscopeclient-linux-x64.tar.gz, extracting to ngscopeclient/, and (with
#              BUILD_TESTING) $DIST_DIR/ngscopeclient-linux-x64-tests.tar.gz: the unit tests and the program
#              folder they need, to run the tests elsewhere
# With no steps, runs all of them but test.
#
# Environment:
#   VCPKG_ROOT            vcpkg checkout [/opt/vcpkg]
#   BUILD_DIR             build directory [build/linux]
#   DIST_DIR              where the package goes [dist]
#   PICO_SDK_INCLUDE_DIR  PicoSDK headers [$BUILD_DIR/pico-sdk-inc, fetched by scripts/ci/fetch-pico-headers.py]
#   BUILD_TESTING         build the unit tests [ON]
#   TEST_DIR              where the test step looks for tests/ [$BUILD_DIR]
#   CCACHE_DIR            set to use ccache

set -euo pipefail

SRC=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$SRC"

: "${VCPKG_ROOT:=/opt/vcpkg}"
: "${BUILD_DIR:=build/linux}"
: "${DIST_DIR:=dist}"
: "${BUILD_TESTING:=ON}"
: "${PICO_SDK_INCLUDE_DIR:=$BUILD_DIR/pico-sdk-inc}"
export VCPKG_ROOT VCPKG_DISABLE_METRICS=1

# The manylinux image's own Python (3.12) rather than EL8's 3.6, which is too old for meson (vcpkg's sdl2/wayland
# ports use it), in case a system package pulled it in
if [ -x /opt/python/cp312-cp312/bin/python3 ]; then
	export PATH="/opt/python/cp312-cp312/bin:$PATH"
fi

TRIPLET=x64-linux-release
INSTALLED="$BUILD_DIR/vcpkg_installed/$TRIPLET"

step_deps()
{
	dnf install -y epel-release
	# X11/Wayland/EGL headers: SDL2's video backends (it loads the libraries at run time). systemd-devel: libudev
	# for libusb and hidapi. dbus-devel: nativefiledialog-extended's portal backend. vulkan-loader-devel: the
	# system's libvulkan.so.1 is linked, not vcpkg's (see msvc/vcpkg.json). mesa-vulkan-drivers: lavapipe for the
	# tests. The rest: what vcpkg's ports need to build.
	dnf install -y --enablerepo=powertools \
		git curl zip unzip tar ninja-build ccache patchelf perl autoconf automake libtool \
		autoconf-archive kernel-headers \
		libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXi-devel libXinerama-devel \
		libXfixes-devel libXScrnSaver-devel libXtst-devel libxkbcommon-devel wayland-devel wayland-protocols-devel \
		mesa-libEGL-devel mesa-libGL-devel mesa-libgbm-devel libdrm-devel ibus-devel \
		dbus-devel systemd-devel \
		vulkan-loader-devel mesa-vulkan-drivers
	if ! command -v cmake > /dev/null || ! cmake --version | grep -qE 'version (3\.(2[1-9]|[3-9][0-9])|[4-9])'; then
		python3 -m pip install --upgrade cmake
	fi
	gcc --version | head -1
	cmake --version | head -1
}

step_vcpkg()
{
	local baseline
	baseline=$(python3 -c "import json; print(json.load(open('msvc/vcpkg.json'))['builtin-baseline'])")
	if [ ! -e "$VCPKG_ROOT/.vcpkg-root" ]; then
		git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
	fi
	if [ "$(git -C "$VCPKG_ROOT" rev-parse HEAD)" != "$baseline" ]; then
		git -C "$VCPKG_ROOT" fetch --quiet origin
		git -C "$VCPKG_ROOT" checkout --quiet "$baseline"
		rm -f "$VCPKG_ROOT/vcpkg"
	fi
	if [ ! -x "$VCPKG_ROOT/vcpkg" ]; then
		"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
	fi
}

step_configure()
{
	if [ ! -e "$PICO_SDK_INCLUDE_DIR/ps6000aApi.h" ]; then
		python3 scripts/ci/fetch-pico-headers.py "$PICO_SDK_INCLUDE_DIR"
	fi

	local features="iio" launcher=()
	if [ "$BUILD_TESTING" = "ON" ]; then
		features="iio;tests"
	fi
	if [ -n "${CCACHE_DIR:-}" ]; then
		launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
	fi

	cmake -S . -B "$BUILD_DIR" -G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
		-DVCPKG_MANIFEST_DIR="$SRC/msvc" \
		-DVCPKG_TARGET_TRIPLET=$TRIPLET \
		-DVCPKG_HOST_TRIPLET=$TRIPLET \
		"-DVCPKG_MANIFEST_FEATURES=$features" \
		-DPKG_CONFIG_EXECUTABLE="$SRC/$INSTALLED/tools/pkgconf/pkgconf" \
		-DVulkan_GLSLC_EXECUTABLE="$SRC/$INSTALLED/tools/shaderc/glslc" \
		-DPICO_SDK_INCLUDE_DIR="$(realpath "$PICO_SDK_INCLUDE_DIR")" \
		-DREQUIRE_PICO=ON \
		-DNFD_PORTAL=ON \
		-DBUILD_TESTING="$BUILD_TESTING" \
		"${launcher[@]}"
}

step_build()
{
	cmake --build "$BUILD_DIR"
}

step_test()
{
	if [ "$BUILD_TESTING" != "ON" ]; then
		echo "BUILD_TESTING is off, no tests to run"
		return
	fi
	local dir="${TEST_DIR:-$BUILD_DIR}" icd t failed=()
	icd=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json 2> /dev/null | head -1 || true)
	if [ -n "$icd" ] && [ -z "${VK_ICD_FILENAMES:-}" ]; then
		echo "Using lavapipe: $icd"
		export VK_ICD_FILENAMES="$icd"
	fi
	# The Catch2 executables directly rather than ctest, so this works on a test bundle too
	for t in "$dir"/tests/*/*; do
		if [ -f "$t" ] && [ -x "$t" ]; then
			echo "--- $t"
			"$t" || failed+=("$(basename "$t")")
		fi
	done
	if [ ${#failed[@]} -gt 0 ]; then
		echo "Failed: ${failed[*]}" >&2
		exit 1
	fi
}

step_package()
{
	local prog="$BUILD_DIR/src/ngscopeclient"
	local dst="$DIST_DIR/ngscopeclient"
	rm -rf "$dst"
	mkdir -p "$dst"

	cp "$prog/ngscopeclient" "$dst/"
	local d
	for d in "$prog"/*/; do
		d=$(basename "$d")
		if [ "$d" != "CMakeFiles" ]; then
			cp -r "$prog/$d" "$dst/"
		fi
	done
	cp "$BUILD_DIR/lib/scopehal/libscopehal.so" "$BUILD_DIR/lib/scopeprotocols/libscopeprotocols.so" "$dst/"
	cp LICENSE "$dst/"

	# Find the libraries next to the program, wherever it's extracted to
	patchelf --set-rpath '$ORIGIN' "$dst/ngscopeclient" "$dst/libscopehal.so" "$dst/libscopeprotocols.so"
	strip --strip-unneeded "$dst/ngscopeclient" "$dst/libscopehal.so" "$dst/libscopeprotocols.so"

	# Licence notices for what's bundled: vcpkg's packages (linked statically, some LGPL) and the third-party code
	# in the tree (imgui etc). One file per package in licenses/.
	local lic="$dst/licenses" c f
	mkdir -p "$lic"
	for c in "$INSTALLED"/share/*/copyright; do
		cp "$c" "$lic/$(basename "$(dirname "$c")").txt"
	done
	for d in lib/*/ src/*/ lib/third_party/*/; do
		f=$(find "$d" -maxdepth 1 -type f -regextype posix-extended -iregex '.*/(licen[cs]e|copying)(\.txt|\.md)?' \
			| sort | head -1)
		if [ -n "$f" ]; then
			cp "$f" "$lic/$(basename "$d").txt"
		fi
	done

	# Nothing should come from the build machine but the libraries every desktop distro has
	echo "Dynamic dependencies:"
	readelf -d "$dst/ngscopeclient" "$dst/libscopehal.so" "$dst/libscopeprotocols.so" | grep NEEDED | sort -u
	echo "Newest glibc symbol version: $(objdump -T "$dst/ngscopeclient" "$dst"/*.so | grep -o 'GLIBC_[0-9.]*' \
		| sort -uV | tail -1)"

	tar -C "$DIST_DIR" -czf "$DIST_DIR/ngscopeclient-linux-x64.tar.gz" ngscopeclient
	ls -l "$DIST_DIR/ngscopeclient-linux-x64.tar.gz"

	if [ "$BUILD_TESTING" = "ON" ]; then
		package_tests
	fi
}

# The test executables find shaders and other data in ../../src/ngscopeclient, as in the build tree
package_tests()
{
	local dst="$DIST_DIR/tests-bundle" t
	rm -rf "$dst"
	mkdir -p "$dst/src"
	cp -r "$DIST_DIR/ngscopeclient" "$dst/src/ngscopeclient"
	for t in "$BUILD_DIR"/tests/*/*; do
		if [ -f "$t" ] && [ -x "$t" ]; then
			mkdir -p "$dst/tests/$(basename "$(dirname "$t")")"
			cp "$t" "$dst/tests/$(basename "$(dirname "$t")")/"
		fi
	done
	for t in "$dst"/tests/*/*; do
		patchelf --set-rpath '$ORIGIN/../../src/ngscopeclient' "$t"
	done
	tar -C "$dst" -czf "$DIST_DIR/ngscopeclient-linux-x64-tests.tar.gz" src tests
	rm -rf "$dst"
	ls -l "$DIST_DIR/ngscopeclient-linux-x64-tests.tar.gz"
}

steps=("$@")
if [ ${#steps[@]} -eq 0 ]; then
	steps=(deps vcpkg configure build package)
fi
for s in "${steps[@]}"; do
	case "$s" in
		deps|vcpkg|configure|build|test|package)
			echo "=== $s"
			"step_$s"
			;;
		*)
			echo "Unknown step $s" >&2
			exit 2
			;;
	esac
done

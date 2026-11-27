#!/bin/bash
# Runs scripts/ci/build-linux.sh in the manylinux_2_28 container the Linux CI uses, with podman (or docker), to
# reproduce the CI build locally. The steps are passed on (default: all but deps, which are baked into a local
# image the first time).
#
#   scripts/ci/linux-container.sh                     # configure, build and package
#   scripts/ci/linux-container.sh build package       # just those steps
#   scripts/ci/linux-container.sh shell               # a shell in the container
#
# vcpkg, its downloads and binary packages, and ccache's cache are kept in $NGSCOPECLIENT_CI_CACHE
# [~/.cache/ngscopeclient-ci], so later runs are much faster. The build goes in build/linux and the package in
# dist/, as in CI.
#
# The tests don't run in the container (EL8's lavapipe can't create a Vulkan instance). As in CI, run them on the
# host from the test bundle the package step makes:
#   mkdir /tmp/t && tar -C /tmp/t -xzf dist/ngscopeclient-linux-x64-tests.tar.gz
#   TEST_DIR=/tmp/t scripts/ci/build-linux.sh test

set -euo pipefail

SRC=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
CACHE=${NGSCOPECLIENT_CI_CACHE:-$HOME/.cache/ngscopeclient-ci}
BASE_IMAGE=quay.io/pypa/manylinux_2_28_x86_64
IMAGE=localhost/ngscopeclient-ci-linux

ENGINE=$(command -v podman || command -v docker || true)
if [ -z "$ENGINE" ]; then
	echo "Needs podman or docker" >&2
	exit 1
fi

mkdir -p "$CACHE/vcpkg" "$CACHE/vcpkg-binary-cache" "$CACHE/ccache"

# With docker, run as the calling user so the build tree isn't owned by root (rootless podman's root already is
# the calling user outside the container)
user=()
if [ "$(basename "$ENGINE")" = "docker" ]; then
	user=(--user "$(id -u):$(id -g)")
fi

if ! "$ENGINE" image inspect "$IMAGE" > /dev/null 2>&1; then
	echo "Making $IMAGE (the CI image with the build dependencies installed)"
	"$ENGINE" pull "$BASE_IMAGE"
	"$ENGINE" rm -f ngscopeclient-ci-deps > /dev/null 2>&1 || true
	"$ENGINE" run --name ngscopeclient-ci-deps -v "$SRC:/src:ro" "$BASE_IMAGE" /src/scripts/ci/build-linux.sh deps
	"$ENGINE" commit ngscopeclient-ci-deps "$IMAGE"
	"$ENGINE" rm ngscopeclient-ci-deps
fi

# Commands can also be piped into the shell step: echo 'ninja -C build/linux -k 0' | scripts/ci/linux-container.sh shell
tty=(-i)
if [ -t 0 ]; then
	tty=(-it)
fi

cmd=(/src/scripts/ci/build-linux.sh "$@")
if [ $# -eq 0 ]; then
	cmd=(/src/scripts/ci/build-linux.sh vcpkg configure build package)
elif [ "$1" = "shell" ]; then
	cmd=(bash)
fi

exec "$ENGINE" run --rm "${tty[@]}" "${user[@]}" \
	-v "$SRC:/src" -w /src \
	-v "$CACHE/vcpkg:/opt/vcpkg" \
	-v "$CACHE/vcpkg-binary-cache:/cache/vcpkg-binary-cache" \
	-v "$CACHE/ccache:/cache/ccache" \
	-e VCPKG_ROOT=/opt/vcpkg \
	-e "VCPKG_BINARY_SOURCES=clear;files,/cache/vcpkg-binary-cache,readwrite" \
	-e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' \
	-e CCACHE_DIR=/cache/ccache \
	-e CCACHE_MAXSIZE=2G \
	-e "BUILD_TESTING=${BUILD_TESTING:-ON}" \
	"$IMAGE" "${cmd[@]}"

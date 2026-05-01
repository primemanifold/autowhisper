#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${AUTOWHISPER_DESKTOP_CI_IMAGE:-autowhisper-desktop-ci:local}"
PLATFORM="${AUTOWHISPER_DOCKER_PLATFORM:-linux/arm64}"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required" >&2
  exit 127
fi

docker build --platform "${PLATFORM}" -t "${IMAGE}" -f "${ROOT}/docker/desktop-ci/Dockerfile" "${ROOT}"

docker run --rm --platform "${PLATFORM}" \
  -v "${ROOT}:/workspace" \
  -w /workspace \
  -e TMPDIR=/tmp \
  "${IMAGE}" \
  bash -lc '
    set -euo pipefail
    rm -rf build-docker-linux build-docker-windows
    cmake -S . -B build-docker-linux -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAUTOWHISPER_ENABLE_TESTS=ON
    cmake --build build-docker-linux --target autowhisper_tests autowhisper -- -j2
    ctest --test-dir build-docker-linux --output-on-failure -R "platform|api/platform|settings|schema|config"

    cmake -S . -B build-docker-windows -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
      -DAUTOWHISPER_ENABLE_TESTS=ON
    cmake --build build-docker-windows --target autowhisper_tests autowhisper -- -j1
    file build-docker-windows/autowhisper.exe build-docker-windows/autowhisper_tests.exe
    echo "Windows cross-build complete. Runtime execution is intentionally left to a Windows host or Wine-capable x86_64 runner."
  '

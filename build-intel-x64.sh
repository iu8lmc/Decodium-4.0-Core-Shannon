#!/bin/bash
# Script to build Decodium3/4 for Intel x86_64 using Docker
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-ft2-ubuntu-x64}"
DOCKERFILE="${DOCKERFILE:-Dockerfile.ubuntu-x64}"
SOURCE_DIR="${SOURCE_DIR:-$(pwd)}"
OUTPUT_DIR="${OUTPUT_DIR:-$(pwd)/build-intel-output}"

if [[ -n "${VERSION:-}" ]]; then
  VERSION="${VERSION#v}"
else
  VERSION="$(tr -d '\r\n' < "${SOURCE_DIR}/fork_release_version.txt" 2>/dev/null || echo "0.0.0-dev")"
fi

BUILD_ID="${BUILD_ID:-$$}"
SOURCE_VOLUME="ft2-intel-source-vol-${BUILD_ID}"
OUTPUT_VOLUME="ft2-intel-build-output-${BUILD_ID}"

cleanup() {
  docker volume rm -f "${SOURCE_VOLUME}" "${OUTPUT_VOLUME}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "=== Building Docker Build Image: ${IMAGE_NAME} ==="
docker build --platform linux/amd64 -t ${IMAGE_NAME} -f ${DOCKERFILE} .

echo "=== Preparing Source and Output Volumes ==="
docker volume create ${SOURCE_VOLUME} > /dev/null || true
docker volume create ${OUTPUT_VOLUME} > /dev/null || true

echo "=== Copying Source to Volume ==="
tar \
  --exclude='.git' \
  --exclude='build' \
  --exclude='build-linux' \
  --exclude='build-arm' \
  --exclude='build-arm-output' \
  --exclude='build-intel-output' \
  -C "${SOURCE_DIR}" \
  -cf - . | docker run --rm -i -v "${SOURCE_VOLUME}:/data" alpine sh -c 'tar -xf - -C /data'

echo "=== Running Build inside Container ==="
docker run --rm -i --platform linux/amd64 \
    -e VERSION="${VERSION}" \
    -v "${SOURCE_VOLUME}:/src:ro" \
    -v "${OUTPUT_VOLUME}:/output" \
    ${IMAGE_NAME} /bin/bash << 'EOF'
        set -e
        echo '>>> Preparing build environment...'
        mkdir -p /build /build-src
        cp -a /src/. /build-src/

        echo '>>> CMake configure...'
        cd /build
        cmake /build-src \
          -DCMAKE_BUILD_TYPE=Release \
          -DWSJT_GENERATE_DOCS=OFF \
          -DWSJT_SKIP_MANPAGES=ON \
          -DWSJT_BUILD_UTILS=OFF \
          -DBUILD_TESTING=OFF \
          -DCMAKE_INSTALL_PREFIX=/tmp/wsjtx_install \
          -DQT_MAJOR_VERSION=6

        echo '>>> Building targets...'
        # Reduced parallelism to avoid OOM killer
        make -j2

        echo '>>> Installing...'
        make install

        echo '>>> Packaging...'
        DIST_DIR=/tmp/wsjtx_dist_intel
        mkdir -p ${DIST_DIR}/bin
        if [ -d /tmp/wsjtx_install/bin ]; then
            cp -r /tmp/wsjtx_install/bin/* ${DIST_DIR}/bin/
        fi

        tar czf /output/decodium4_ubuntu_x86_64.tar.gz -C /tmp wsjtx_dist_intel
        
        echo '>>> Generating DEB package...'
        cpack -G DEB
        cp *.deb /output/ 2>/dev/null || echo "No .deb files found"

        echo '>>> Generating AppImage...'
        export ARCH=x86_64
        
        # Download linuxdeploy and qt plugin if not present
        if [ ! -f linuxdeploy-x86_64.AppImage ]; then
            echo ">>>> Downloading linuxdeploy..."
            wget -q -O linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
            chmod +x linuxdeploy-x86_64.AppImage
        fi
        
        
        echo ">>>> Patching linuxdeploy AppImage for Docker/Rosetta compatibility..."
        dd if=/dev/zero of=linuxdeploy-x86_64.AppImage bs=1 seek=8 count=3 conv=notrunc >/dev/null 2>&1
        export LINUXDEPLOY_BIN="./linuxdeploy-x86_64.AppImage --appimage-extract-and-run"

        if [ ! -f linuxdeploy-plugin-qt-x86_64.AppImage ]; then
             echo ">>>> Downloading linuxdeploy-plugin-qt..."
             wget -q -O linuxdeploy-plugin-qt-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
             chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
             dd if=/dev/zero of=linuxdeploy-plugin-qt-x86_64.AppImage bs=1 seek=8 count=3 conv=notrunc >/dev/null 2>&1
        fi
        export LINUXDEPLOY_PLUGINS_DIR=.

        sed -i 's/^Exec=wsjtx/Exec=ft2/' /build-src/wsjtx.desktop
        sed -i 's/^Name=wsjtx/Name=Decodium FT2/' /build-src/wsjtx.desktop

        # Run with --appimage-extract-and-run for the plugin as well if needed
        # but linuxdeploy handles plugins. 
        # We might need to extract the plugin too if it fails.
        
        $LINUXDEPLOY_BIN --appdir /tmp/AppDir \
            --executable /tmp/wsjtx_install/bin/ft2 \
            --desktop-file /build-src/wsjtx.desktop \
            --icon-file /build-src/icons/Unix/wsjtx_icon.png \
            --plugin qt \
            --output appimage

        APPIMAGE_NAME="decodium4-ft2-${VERSION}-linux-x86_64.AppImage"
        APPIMAGE_SRC="$(find . -maxdepth 1 -name '*.AppImage' ! -name 'linuxdeploy*.AppImage' ! -name 'appimagetool*.AppImage' | head -n1)"
        if [ -n "${APPIMAGE_SRC}" ]; then
            mv "${APPIMAGE_SRC}" "/output/${APPIMAGE_NAME}"
            sha256sum "/output/${APPIMAGE_NAME}" > "/output/${APPIMAGE_NAME}.sha256.txt"
        else
            echo "No .AppImage files found"
        fi

        echo '>>> Build Complete!'
EOF

echo "=== Extracting Output from Volume ==="
mkdir -p "${OUTPUT_DIR}"
TEMP_CONTAINER=$(docker create -v ${OUTPUT_VOLUME}:/data alpine)
docker cp ${TEMP_CONTAINER}:/data/. "${OUTPUT_DIR}/"
docker rm ${TEMP_CONTAINER} > /dev/null

echo "=== DONE! ==="
ls -lh "${OUTPUT_DIR}"/decodium4* || true

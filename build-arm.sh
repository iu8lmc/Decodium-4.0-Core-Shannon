#!/bin/bash
# Script to build Decodium 4 for ARM using Docker
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-ft2-trixie-arm}"
DOCKERFILE="${DOCKERFILE:-Dockerfile.trixie-arm}"
SOURCE_DIR="${SOURCE_DIR:-$(pwd)}"
OUTPUT_DIR="${OUTPUT_DIR:-$(pwd)/build-arm-output}"
if [[ -n "${VERSION:-}" ]]; then
  VERSION="${VERSION#v}"
else
  VERSION="$(tr -d '\r\n' < "${SOURCE_DIR}/fork_release_version.txt" 2>/dev/null || echo "0.0.0-dev")"
fi
BUILD_ID="${BUILD_ID:-$$}"
SOURCE_VOLUME="ft2-source-vol-${BUILD_ID}"
OUTPUT_VOLUME="ft2-build-output-${BUILD_ID}"

cleanup() {
  docker volume rm -f "${SOURCE_VOLUME}" "${OUTPUT_VOLUME}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "=== Building Docker Build Image: ${IMAGE_NAME} ==="
docker build --platform linux/arm64 -t ${IMAGE_NAME} -f ${DOCKERFILE} .

echo "=== Preparing Source and Output Volumes ==="
docker volume create ${SOURCE_VOLUME} > /dev/null || true
docker volume create ${OUTPUT_VOLUME} > /dev/null || true

echo "=== Copying Source to Volume ==="
# We copy only the necessary files to the volume to avoid bloat.
COPYFILE_DISABLE=1 tar \
  --exclude='.git' \
  --exclude='build' \
  --exclude='build-appimage' \
  --exclude='build-utils' \
  --exclude='build-arm-output' \
  -C "${SOURCE_DIR}" \
  -cf - . | docker run --rm -i -v "${SOURCE_VOLUME}:/data" alpine sh -c 'tar -xf - -C /data'

echo "=== Running Build inside Container ==="
# We use a heredoc to avoid quoting issues between host and container shells
docker run --rm -i --platform linux/arm64 \
    -e VERSION="${VERSION}" \
    -v "${SOURCE_VOLUME}:/src:ro" \
    -v "${OUTPUT_VOLUME}:/output" \
    ${IMAGE_NAME} /bin/bash << 'EOF'
        set -e
        echo '>>> Preparing build environment...'
        mkdir -p /build /build-src
        # Copy source to a writable location
        cp -a /src/. /build-src/

        echo '>>> Patching source...'
        # rig_get_conf2 fallback
        sed -i 's/rig_get_conf2 (rig_.data (), token, value.data (),value.length())/rig_get_conf (rig_.data (), token, value.data ())/' \
          /build-src/Transceiver/HamlibTransceiver.cpp

        # PSKReporter 5th arg
        sed -i 's/m_psk_Reporter.setLocalStation(m_config.my_callsign (), m_config.my_grid (), antenna_description, rig_information);/m_psk_Reporter.setLocalStation(m_config.my_callsign (), m_config.my_grid (), antenna_description, rig_information, QString{"Decodium 3 FT2 v" + version() + " " + m_revision}.simplified());/' \
          /build-src/widgets/mainwindow.cpp

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
        # Reduced parallelism to avoid OOM killer on memory-intensive files like qcustomplot
        make -j2

        echo '>>> Installing to temporary prefix...'
        make install

        echo '>>> Packaging...'
        DIST_DIR=/tmp/wsjtx_dist_arm
        mkdir -p ${DIST_DIR}/bin
        # Only copy what we need
        if [ -d /tmp/wsjtx_install/bin ]; then
            cp -r /tmp/wsjtx_install/bin/* ${DIST_DIR}/bin/
        fi

        tar czf /output/decodium4_trixie_arm64.tar.gz -C /tmp wsjtx_dist_arm
        echo '>>> Tarball Complete! Output at /output/decodium4_trixie_arm64.tar.gz'

        echo '>>> Generating DEB package...'
        # Running cpack in the build directory
        cpack -G DEB
        cp *.deb /output/ 2>/dev/null || echo "No .deb files found"

        echo '>>> Generating AppImage...'
        export ARCH=aarch64
        export QMAKE="$(command -v qmake6)"
        export QML_SOURCES_PATHS=/build-src/qml

        cat > /tmp/decodium.desktop <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Decodium 4
Comment=Amateur Radio Weak Signal Operating
Exec=decodium
Icon=decodium
Terminal=false
Categories=AudioVideo;Audio;HamRadio;
StartupNotify=true
DESKTOP

        cp /build-src/icons/Unix/decodium_icon.png /tmp/decodium.png

        linuxdeploy --appdir /tmp/AppDir \
            --executable /tmp/wsjtx_install/bin/decodium \
            --desktop-file /tmp/decodium.desktop \
            --icon-file /tmp/decodium.png \
            --plugin qt

        mkdir -p /tmp/AppDir/usr/bin/qml
        QT_QML_DIR="$(qmake6 -query QT_INSTALL_QML 2>/dev/null || true)"
        if [ -n "${QT_QML_DIR}" ] && [ -d "${QT_QML_DIR}" ]; then
            cp -a "${QT_QML_DIR}/." /tmp/AppDir/usr/bin/qml/
        fi
        cp -a /tmp/wsjtx_install/bin/qml/. /tmp/AppDir/usr/bin/qml/
        test -f /tmp/AppDir/usr/bin/qml/QtQuick/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQuick/Controls/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQuick/Controls/Material/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQuick/Dialogs/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQuick/Layouts/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQuick/Templates/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQuick/Window/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtCore/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQml/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQml/Models/qmldir
        test -f /tmp/AppDir/usr/bin/qml/QtQml/WorkerScript/qmldir
        find /tmp/AppDir -name '._*' -o -name '.DS_Store' | xargs -r rm -f

        linuxdeploy --appdir /tmp/AppDir --output appimage

        APPIMAGE_NAME="decodium4-ft2-${VERSION}-linux-aarch64.AppImage"
        APPIMAGE_SRC="$(find . -maxdepth 1 -name '*.AppImage' ! -name 'linuxdeploy*.AppImage' ! -name 'appimagetool*.AppImage' | head -n1)"
        if [ -n "${APPIMAGE_SRC}" ]; then
            mv "${APPIMAGE_SRC}" "/output/${APPIMAGE_NAME}"
            sha256sum "/output/${APPIMAGE_NAME}" > "/output/${APPIMAGE_NAME}.sha256.txt"
        else
            echo "No .AppImage files found"
            exit 1
        fi

        echo '>>> Build Complete!'
EOF

echo "=== Extracting Output from Volume ==="
mkdir -p "${OUTPUT_DIR}"
# We create a temporary container to copy the files out
TEMP_CONTAINER=$(docker create -v ${OUTPUT_VOLUME}:/data alpine)
docker cp ${TEMP_CONTAINER}:/data/. "${OUTPUT_DIR}/"
docker rm ${TEMP_CONTAINER} > /dev/null

echo "=== DONE! ==="
ls -lh "${OUTPUT_DIR}"/decodium4*

#!/bin/bash
# Script to build Decodium3/4 for Intel x86_64 using Docker
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-ft2-ubuntu-x64-qt6}"
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
COPYFILE_DISABLE=1 tar \
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
        export QMAKE="$(command -v qmake6)"
        export QML_SOURCES_PATHS=/build-src/qml

        # Download linuxdeploy and qt plugin if not present
        if [ ! -f linuxdeploy-x86_64.AppImage ]; then
            echo ">>>> Downloading linuxdeploy..."
            wget -q -O linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
            chmod +x linuxdeploy-x86_64.AppImage
        fi


        echo ">>>> Patching linuxdeploy AppImage for Docker/Rosetta compatibility..."
        dd if=/dev/zero of=linuxdeploy-x86_64.AppImage bs=1 seek=8 count=3 conv=notrunc >/dev/null 2>&1
        export LINUXDEPLOY_BIN="./linuxdeploy-x86_64.AppImage --appimage-extract-and-run"

        mkdir -p linuxdeploy-tools
        if [ ! -f linuxdeploy-tools/linuxdeploy-plugin-qt-x86_64.AppImage ]; then
             echo ">>>> Downloading linuxdeploy-plugin-qt..."
             wget -q -O linuxdeploy-tools/linuxdeploy-plugin-qt-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
             chmod +x linuxdeploy-tools/linuxdeploy-plugin-qt-x86_64.AppImage
             dd if=/dev/zero of=linuxdeploy-tools/linuxdeploy-plugin-qt-x86_64.AppImage bs=1 seek=8 count=3 conv=notrunc >/dev/null 2>&1
        fi
        echo ">>>> Extracting linuxdeploy-plugin-qt for Docker/Rosetta compatibility..."
        rm -rf linuxdeploy-plugin-qt-extracted linuxdeploy-plugin-qt squashfs-root
        linuxdeploy-tools/linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract >/dev/null
        mv squashfs-root linuxdeploy-plugin-qt-extracted
        cat > linuxdeploy-plugin-qt <<'PLUGIN'
#!/bin/sh
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec "$ROOT_DIR/linuxdeploy-plugin-qt-extracted/AppRun" "$@"
PLUGIN
        chmod +x linuxdeploy-plugin-qt
        export PATH="$(pwd):${PATH}"

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

        # First populate the AppDir and let the Qt plugin collect Qt/QML imports.
        $LINUXDEPLOY_BIN --appdir /tmp/AppDir \
            --executable /tmp/wsjtx_install/bin/decodium \
            --desktop-file /tmp/decodium.desktop \
            --icon-file /tmp/decodium.png \
            --plugin qt

        # Keep Qt imports available beside the executable. Some linuxdeploy
        # builds miss Qt 6 QML modules, while Decodium loads QML from
        # applicationDirPath()/qml.
        mkdir -p /tmp/AppDir/usr/bin/qml
        QT_QML_DIR="$(${QMAKE} -query QT_INSTALL_QML 2>/dev/null || true)"
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

        $LINUXDEPLOY_BIN --appdir /tmp/AppDir --output appimage

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

#!/bin/bash
set -e

echo "============================================="
echo " WSJTX 3.0 Linux x86_64 Docker Build"
echo "============================================="

export DEBIAN_FRONTEND=noninteractive

echo ""
echo ">>> [1/7] Installing build dependencies..."
apt-get update -qq
apt-get install -y -qq \
  build-essential \
  gfortran \
  cmake \
  pkg-config \
  gzip \
  qtbase5-dev \
  qttools5-dev \
  qttools5-dev-tools \
  libqt5serialport5-dev \
  qtmultimedia5-dev \
  libqt5websockets5-dev \
  libqt5sql5-sqlite \
  libfftw3-dev \
  libhamlib-dev \
  libhamlib-utils \
  libboost-log-dev \
  libusb-1.0-0-dev \
  libudev-dev \
  > /dev/null 2>&1

echo "    Dependencies installed OK"

echo ""
echo ">>> [2/7] Copying source tree..."
cp -a /src /build-src
echo "    Source copied to /build-src"

echo ""
echo ">>> [2b/7] Patching for Hamlib 4.3.x compatibility..."
# rig_get_conf2 doesn't exist in Hamlib < 4.5; fall back to rig_get_conf
sed -i 's/rig_get_conf2 (rig_.data (), token, value.data (),value.length())/rig_get_conf (rig_.data (), token, value.data ())/' \
  /build-src/Transceiver/HamlibTransceiver.cpp
echo "    Patched HamlibTransceiver.cpp"

# PSKReporter::setLocalStation now takes 5 args (added program_info) but call only passes 4
sed -i 's/m_psk_Reporter.setLocalStation(m_config.my_callsign (), m_config.my_grid (), antenna_description, rig_information);/m_psk_Reporter.setLocalStation(m_config.my_callsign (), m_config.my_grid (), antenna_description, rig_information, QString{"Decodium 3 FT2 v" + version() + " " + m_revision}.simplified());/' \
  /build-src/widgets/mainwindow.cpp
echo "    Patched mainwindow.cpp (PSKReporter 5th arg)"

echo ""
echo ">>> [3/7] CMake configure..."
mkdir -p /build
cd /build
cmake /build-src \
  -DCMAKE_BUILD_TYPE=Release \
  -DWSJT_GENERATE_DOCS=OFF \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_BUILD_UTILS=OFF \
  -DCMAKE_INSTALL_PREFIX=/tmp/wsjtx_install \
  2>&1

echo ""
echo ">>> [4/7] Building Fortran libraries (single-threaded)..."
make -j1 wsjt_fort wsjt_fort_omp wsjt_cxx 2>&1

echo ""
echo ">>> [5/7] Building all targets..."
make -j$(nproc) 2>&1

echo ""
echo ">>> [6/7] Installing..."
make install 2>&1

echo ""
echo ">>> [7/7] Creating distribution package..."

DIST_DIR=/tmp/wsjtx_dist_linux
mkdir -p ${DIST_DIR}/bin
mkdir -p ${DIST_DIR}/share

# Copy binaries and strip them
for exe in wsjtx jt9 wsprd message_aggregator udp_daemon wsjtx_app_version fmtave fcal fmeasure; do
  if [ -f /tmp/wsjtx_install/bin/${exe} ]; then
    cp /tmp/wsjtx_install/bin/${exe} ${DIST_DIR}/bin/
    strip ${DIST_DIR}/bin/${exe} 2>/dev/null || true
    echo "    Packed: ${exe}"
  fi
done

# Copy rigctl utilities
for exe in rigctl-wsjtx rigctld-wsjtx rigctlcom-wsjtx; do
  if [ -f /tmp/wsjtx_install/bin/${exe} ]; then
    cp /tmp/wsjtx_install/bin/${exe} ${DIST_DIR}/bin/
    strip ${DIST_DIR}/bin/${exe} 2>/dev/null || true
    echo "    Packed: ${exe}"
  fi
done

# Copy ALLCALL7.TXT
if [ -f /tmp/wsjtx_install/bin/ALLCALL7.TXT ]; then
  cp /tmp/wsjtx_install/bin/ALLCALL7.TXT ${DIST_DIR}/bin/
fi

# Copy sounds directory
if [ -d /tmp/wsjtx_install/bin/sounds ]; then
  cp -r /tmp/wsjtx_install/bin/sounds ${DIST_DIR}/bin/
  echo "    Packed: sounds/"
fi

# Copy data files
if [ -d /tmp/wsjtx_install/share/wsjtx ]; then
  cp -r /tmp/wsjtx_install/share/wsjtx ${DIST_DIR}/share/
  echo "    Packed: share/wsjtx/"
fi

# Copy doc files
if [ -d /tmp/wsjtx_install/share/doc ]; then
  cp -r /tmp/wsjtx_install/share/doc ${DIST_DIR}/share/
  echo "    Packed: share/doc/"
fi

# Create launcher script
cat > ${DIST_DIR}/wsjtx.sh << 'LAUNCHER'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="${SCRIPT_DIR}/bin:${PATH}"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
exec "${SCRIPT_DIR}/bin/wsjtx" "$@"
LAUNCHER
chmod +x ${DIST_DIR}/wsjtx.sh

# Create runtime dependency installer
cat > ${DIST_DIR}/install-deps.sh << 'DEPS'
#!/bin/bash
# Install runtime dependencies for WSJTX 3.0 on Ubuntu/Debian
set -e
echo "Installing WSJTX 3.0 runtime dependencies..."
sudo apt-get update
sudo apt-get install -y \
  libqt5widgets5 \
  libqt5serialport5 \
  libqt5multimedia5 \
  libqt5multimedia5-plugins \
  libqt5websockets5 \
  libqt5sql5-sqlite \
  libqt5printsupport5 \
  libqt5network5 \
  libqt5core5a \
  libqt5gui5 \
  libfftw3-single3 \
  libhamlib4 \
  libhamlib-utils \
  libboost-log1.74.0 \
  libusb-1.0-0 \
  libgomp1 \
  libgfortran5
echo ""
echo "Dependencies installed. Run ./wsjtx.sh to start WSJTX 3.0"
DEPS
chmod +x ${DIST_DIR}/install-deps.sh

# Create tarball
cd /tmp
tar czf /output/wsjtx_3.0_linux_x86_64.tar.gz wsjtx_dist_linux/

echo ""
echo "============================================="
echo " BUILD COMPLETE!"
echo "============================================="
echo ""
ls -lh /output/wsjtx_3.0_linux_x86_64.tar.gz
echo ""
echo "Contents:"
tar tzf /output/wsjtx_3.0_linux_x86_64.tar.gz | head -30
echo "..."
echo ""
echo "Binaries:"
file ${DIST_DIR}/bin/wsjtx

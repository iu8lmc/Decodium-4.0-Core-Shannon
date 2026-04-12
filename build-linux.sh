#!/bin/bash
#
# WSJTX 3.0 - Linux Build Script
# Compila e crea pacchetto distribuibile per Linux x86_64
#
# Uso:
#   1. Copiare il sorgente WSJTX su una macchina Linux (Ubuntu 22.04+)
#   2. chmod +x build-linux.sh
#   3. ./build-linux.sh
#
# Requisiti: Ubuntu 22.04+ o Debian 12+
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-linux"
DIST_DIR="$SCRIPT_DIR/wsjtx_dist_linux"
JOBS=$(nproc 2>/dev/null || echo 2)

echo "============================================"
echo " WSJTX 3.0 - Linux Build"
echo " Source: $SCRIPT_DIR"
echo " Build:  $BUILD_DIR"
echo " Jobs:   $JOBS"
echo "============================================"

# ---- Step 1: Install dependencies ----
echo ""
echo "[1/5] Installing build dependencies..."

sudo apt-get update -qq

sudo apt-get install -y -qq \
    build-essential \
    gfortran \
    cmake \
    qtbase5-dev \
    qtmultimedia5-dev \
    libqt5serialport5-dev \
    libqt5websockets5-dev \
    libqt5svg5-dev \
    libqt5sql5-sqlite \
    qttools5-dev \
    qttools5-dev-tools \
    libfftw3-dev \
    libhamlib-dev \
    libhamlib-utils \
    libusb-1.0-0-dev \
    libboost-log-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    portaudio19-dev \
    libreadline-dev \
    texinfo \
    asciidoctor \
    pkg-config \
    2>/dev/null || {
        echo "WARNING: asciidoctor not found, will disable docs"
        DISABLE_DOCS=1
    }

echo "[1/5] Dependencies installed."

# ---- Step 2: Configure ----
echo ""
echo "[2/5] Configuring CMake..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CMAKE_OPTS=(
    -G "Unix Makefiles"
    -DCMAKE_INSTALL_PREFIX=/usr/local
    -DCMAKE_BUILD_TYPE=Release
)

# Disable docs if asciidoctor not available
if [ "${DISABLE_DOCS:-0}" = "1" ] || ! command -v asciidoctor &>/dev/null; then
    CMAKE_OPTS+=(-DWSJT_GENERATE_DOCS=OFF)
fi

cmake "${CMAKE_OPTS[@]}" "$SCRIPT_DIR"

echo "[2/5] CMake configured."

# ---- Step 3: Build ----
echo ""
echo "[3/5] Building (this may take a while)..."

# Fortran modules need sequential build first
make -j1 wsjt_fort_omp 2>&1 | tail -5
# Then parallel for the rest
make -j"$JOBS" wsjtx jt9 wsprd message_aggregator 2>&1 | tail -10

echo "[3/5] Build complete."

# Verify
echo ""
echo "Executables:"
file wsjtx jt9 wsprd message_aggregator 2>/dev/null | grep -o '.*ELF.*'
ls -lh wsjtx jt9 wsprd message_aggregator

# ---- Step 4: Create distribution package ----
echo ""
echo "[4/5] Creating distribution package..."

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/bin"
mkdir -p "$DIST_DIR/share/wsjtx"

# Executables
cp wsjtx jt9 wsprd message_aggregator "$DIST_DIR/bin/"
strip "$DIST_DIR/bin/"* 2>/dev/null || true

# Data files
cp "$SCRIPT_DIR/cty.dat" "$DIST_DIR/share/wsjtx/" 2>/dev/null || true
cp -r "$SCRIPT_DIR/sounds" "$DIST_DIR/share/wsjtx/" 2>/dev/null || true
cp -r "$SCRIPT_DIR/Palettes" "$DIST_DIR/share/wsjtx/" 2>/dev/null || true

# Launcher script
cat > "$DIST_DIR/wsjtx.sh" << 'LAUNCHER'
#!/bin/bash
# WSJTX Launcher
DIR="$(cd "$(dirname "$0")" && pwd)"
export PATH="$DIR/bin:$PATH"
export WSJTX_DATA_DIR="$DIR/share/wsjtx"
exec "$DIR/bin/wsjtx" "$@"
LAUNCHER
chmod +x "$DIST_DIR/wsjtx.sh"

# List shared library dependencies (for reference)
echo ""
echo "Shared library dependencies (must be installed on target):"
ldd "$DIST_DIR/bin/wsjtx" 2>/dev/null | grep -v "linux-vdso\|ld-linux" | awk '{print $1}' | sort -u > "$DIST_DIR/DEPENDENCIES.txt"
cat "$DIST_DIR/DEPENDENCIES.txt"

# Install script for dependencies on target machine
cat > "$DIST_DIR/install-deps.sh" << 'DEPS'
#!/bin/bash
# Install runtime dependencies for WSJTX on Ubuntu/Debian
echo "Installing WSJTX runtime dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq \
    libqt5core5a \
    libqt5gui5 \
    libqt5widgets5 \
    libqt5network5 \
    libqt5multimedia5 \
    libqt5serialport5 \
    libqt5websockets5 \
    libqt5printsupport5 \
    libqt5sql5 \
    libqt5sql5-sqlite \
    libqt5svg5 \
    libfftw3-single3 \
    libhamlib4 \
    libusb-1.0-0 \
    libboost-log1.74.0 \
    libboost-filesystem1.74.0 \
    libportaudio2 \
    libgfortran5 \
    libgomp1
echo "Done. Run ./wsjtx.sh to start."
DEPS
chmod +x "$DIST_DIR/install-deps.sh"

echo "[4/5] Package created."

# ---- Step 5: Create tarball ----
echo ""
echo "[5/5] Creating tarball..."

cd "$SCRIPT_DIR"
TARBALL="wsjtx_3.0_linux_x86_64.tar.gz"
tar czf "$TARBALL" -C "$(dirname "$DIST_DIR")" "$(basename "$DIST_DIR")"

echo ""
echo "============================================"
echo " BUILD COMPLETE"
echo "============================================"
echo ""
echo " Tarball: $SCRIPT_DIR/$TARBALL"
echo " Size:    $(du -sh "$TARBALL" | cut -f1)"
echo ""
echo " To deploy on target Linux machine:"
echo "   1. tar xzf $TARBALL"
echo "   2. cd wsjtx_dist_linux"
echo "   3. ./install-deps.sh    # installs runtime libs"
echo "   4. ./wsjtx.sh           # run WSJTX"
echo ""

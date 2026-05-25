#!/usr/bin/env bash
# Build Helm module for Move Anything (ARM64)
#
# Uses CMake to build the Helm core engine and plugin wrapper.
# Automatically uses Docker for cross-compilation if needed.
# Set CROSS_PREFIX to skip Docker (e.g., for native ARM builds).
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
IMAGE_NAME="move-anything-helm-builder"

# Check if we need Docker
if [ -z "$CROSS_PREFIX" ] && [ ! -f "/.dockerenv" ]; then
    echo "=== Helm Module Build (via Docker) ==="
    echo ""

    # Build Docker image if needed
    if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
        echo "Building Docker image (first time only)..."
        docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile" "$REPO_ROOT"
        echo ""
    fi

    # Run build inside container
    echo "Running build..."
    docker run --rm \
        -v "$REPO_ROOT:/build" \
        -u "$(id -u):$(id -g)" \
        -w /build \
        "$IMAGE_NAME" \
        ./scripts/build.sh

    echo ""
    echo "=== Done ==="
    exit 0
fi

# === Actual build (runs in Docker or with cross-compiler) ===
cd "$REPO_ROOT"

echo "=== Building Helm Module ==="

# Create build directory
mkdir -p build

# Run CMake configure with cross-compilation toolchain
echo "Configuring CMake..."
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja \
    2>&1

# Build
echo "Building (this may take a while)..."
cmake --build build --target surge-move-plugin -j$(nproc) 2>&1

# Package
echo "Packaging..."
mkdir -p dist/helm

# Copy files to dist
cat src/module.json > dist/helm/module.json
[ -f src/help.json ] && cat src/help.json > dist/helm/help.json
[ -f LICENSE ] && cat LICENSE > dist/helm/LICENSE
[ -f NOTICE ]  && cat NOTICE  > dist/helm/NOTICE
cat src/ui.js > dist/helm/ui.js
cat build/dsp.so > dist/helm/dsp.so
chmod +x dist/helm/dsp.so

# Copy Helm factory presets
if [ -d "src/dsp/helm/patches" ]; then
    echo "Copying Helm factory patches..."
    mkdir -p dist/helm/helm-data/patches
    cp -r "src/dsp/helm/patches/Factory Presets" dist/helm/helm-data/patches/
    mkdir -p "dist/helm/helm-data/patches/Factory Presets/Keys"
    cp "data/Move Organ.helm" "dist/helm/helm-data/patches/Factory Presets/Keys/"
fi

# Create tarball for release
cd dist
tar -czvf helm-module.tar.gz helm/
cd ..

echo ""
echo "=== Build Complete ==="
echo "Output: dist/helm/"
echo "Tarball: dist/helm-module.tar.gz"
echo ""
echo "To install on Move:"
echo "  ./scripts/install.sh"

#!/bin/bash
# Install Helm module to Move
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$REPO_ROOT"

if [ ! -d "dist/helm" ]; then
    echo "Error: dist/helm not found. Run ./scripts/build.sh first."
    exit 1
fi

echo "=== Installing Helm Module ==="

# Deploy to Move
echo "Copying module to Move..."
ssh ableton@move.local "mkdir -p /data/UserData/schwung/modules/sound_generators/helm"
scp -r dist/helm/* ableton@move.local:/data/UserData/schwung/modules/sound_generators/helm/

# Install chain presets if they exist
if [ -d "src/chain_patches" ]; then
    echo "Installing chain presets..."
    scp src/chain_patches/*.json ableton@move.local:/data/UserData/schwung/patches/
fi

# Set permissions
echo "Setting permissions..."
ssh ableton@move.local "chmod -R a+rw /data/UserData/schwung/modules/sound_generators/helm"

echo ""
echo "=== Install Complete ==="
echo "Module installed to: /data/UserData/schwung/modules/sound_generators/helm/"
echo ""
echo "Restart Move Anything to load the new module."

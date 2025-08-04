#!/bin/bash

set -e

echo "🔧 Setting up OpenCV environment for Phase 2.3..."

# Check if OpenCV is installed
if ! pkg-config --exists opencv4; then
    echo "❌ OpenCV not found. Installing..."
    sudo apt update
    sudo apt install -y libopencv-dev libopencv-contrib-dev
else
    echo "✅ OpenCV found: $(pkg-config --modversion opencv4)"
fi

# Build with OpenCV
echo "🔨 Building vision service with OpenCV..."
cd "$(dirname "$0")/../vision-service"
make clean
make build-opencv

echo "🧪 Running OpenCV tests..."
make test-opencv

echo "✅ OpenCV setup complete!"
echo "🚀 To run with OpenCV: make run-opencv"
echo "🔗 For integration tests: make test-integration-opencv"
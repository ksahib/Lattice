#!/bin/bash

# Build script for worker_server using vcpkg gRPC/protobuf installation.
# Usage:
#   cd Lattice/Worker
#   chmod +x build_worker.sh
#   ./build_worker.sh

set -e

# Try to find vcpkg installation (same logic as try_grpc2/build.sh)
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

if [ ! -d "$VCPKG_ROOT" ]; then
    if [ -d "../../../../vcpkg" ]; then
        VCPKG_ROOT="../../../../vcpkg"
    elif [ -d "$HOME/vcpkg" ]; then
        VCPKG_ROOT="$HOME/vcpkg"
    elif [ -d "/opt/vcpkg" ]; then
        VCPKG_ROOT="/opt/vcpkg"
    else
        echo "Error: vcpkg not found. Please set VCPKG_ROOT environment variable."
        echo "Example: export VCPKG_ROOT=\$HOME/vcpkg"
        exit 1
    fi
fi

VCPKG_INSTALLED="$VCPKG_ROOT/installed/x64-linux"

if [ ! -d "$VCPKG_INSTALLED" ]; then
    echo "Error: vcpkg installed directory not found at $VCPKG_INSTALLED"
    echo "Make sure you have installed packages with: vcpkg install grpc protobuf"
    exit 1
fi

echo "Using vcpkg at: $VCPKG_ROOT"
echo "Installed packages at: $VCPKG_INSTALLED"

# Compile flags
CXXFLAGS="-std=c++17 -O2"
# Include vcpkg headers + system jsoncpp headers
INCLUDES="-I$VCPKG_INSTALLED/include -I/usr/include/jsoncpp"
LIBS="-L$VCPKG_INSTALLED/lib"

# Collect all abseil libraries (same trick as try_grpc2/build.sh)
ABSEIL_LIBS=$(find "$VCPKG_INSTALLED/lib" -name "libabsl*.a" -exec basename {} \; | sed 's|lib||; s|\.a||' | tr '\n' ' ')

# Build link command with all required libraries
# Use --start-group and --end-group to handle circular dependencies
LINK_LIBS="-Wl,--start-group"
LINK_LIBS="$LINK_LIBS -lgrpc++ -lgrpc -lgpr -lprotobuf -lre2"

# Add all abseil libraries
for lib in $ABSEIL_LIBS; do
    LINK_LIBS="$LINK_LIBS -l$lib"
done

# Add upb libraries (required by gRPC)
LINK_LIBS="$LINK_LIBS -laddress_sorting -lupb_textformat_lib -lupb_json_lib -lupb_wire_lib -lupb_message_lib -lupb_mini_descriptor_lib -lupb_mem_lib -lupb_base_lib"

# Add utf8_range libraries (required by protobuf)
LINK_LIBS="$LINK_LIBS -lutf8_range -lutf8_validity"

LINK_LIBS="$LINK_LIBS -Wl,--end-group"
LINK_LIBS="$LINK_LIBS -lz -lcares -lssl -lcrypto -pthread -ldl -ljsoncpp -lcurl"

echo ""
echo "Compiling worker_server..."
g++ $CXXFLAGS $INCLUDES -o worker_server \
    worker_server.cpp \
    task_service.pb.cc task_service.grpc.pb.cc \
    $LIBS $LINK_LIBS

echo ""
echo "✓ worker_server build successful!"
echo "Run it like:"
echo "  ./worker_server http://127.0.0.1:8080 127.0.0.1 50051"



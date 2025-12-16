#!/bin/bash

# Link final transformed LLVM IR with gRPC-based parallel runtime.
# Usage: ./link_with_grpc.sh /path/to/file.opt.ll

set -e

if [ -z "$1" ]; then
  echo "Usage: $0 <input.opt.ll>"
  exit 1
fi

INPUT_LL="$1"
OUT_BIN="runprog"

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

echo "[link_with_grpc] Using vcpkg at: $VCPKG_ROOT"
echo "[link_with_grpc] Installed packages at: $VCPKG_INSTALLED"

CXXFLAGS="-std=c++17 -O2"
# Get the absolute path to Master directory (where this script lives)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKER_DIR="$SCRIPT_DIR/../Worker"

if [ ! -d "$WORKER_DIR" ]; then
  echo "Error: Worker directory not found at $WORKER_DIR"
  exit 1
fi

# Verify task_service.grpc.pb.h exists
if [ ! -f "$WORKER_DIR/task_service.grpc.pb.h" ]; then
  echo "Error: task_service.grpc.pb.h not found at $WORKER_DIR/task_service.grpc.pb.h"
  exit 1
fi

INCLUDES="-I$VCPKG_INSTALLED/include -I$WORKER_DIR -I/usr/include/jsoncpp"
LIBS="-L$VCPKG_INSTALLED/lib"

# Collect all abseil libraries
ABSEIL_LIBS=$(find "$VCPKG_INSTALLED/lib" -name "libabsl*.a" -exec basename {} \; | sed 's|lib||; s|\.a||' | tr '\n' ' ')

# Build link command with all required libraries
LINK_LIBS="-Wl,--start-group"
LINK_LIBS="$LINK_LIBS -lgrpc++ -lgrpc -lgpr -lprotobuf -lre2"
for lib in $ABSEIL_LIBS; do
  LINK_LIBS="$LINK_LIBS -l$lib"
done
LINK_LIBS="$LINK_LIBS -laddress_sorting -lupb_textformat_lib -lupb_json_lib -lupb_wire_lib -lupb_message_lib -lupb_mini_descriptor_lib -lupb_mem_lib -lupb_base_lib"
LINK_LIBS="$LINK_LIBS -lutf8_range -lutf8_validity"
LINK_LIBS="$LINK_LIBS -Wl,--end-group"
LINK_LIBS="$LINK_LIBS -lz -lcares -lssl -lcrypto -pthread -ldl -ljsoncpp"

echo "[link_with_grpc] Compiling parallel_runtime_grpc.cpp and task_service protobuf files ..."
g++ $CXXFLAGS $INCLUDES -c parallel_runtime_grpc.cpp -o parallel_runtime_grpc.o
g++ $CXXFLAGS $INCLUDES -c "$WORKER_DIR/task_service.pb.cc" -o task_service.pb.o
g++ $CXXFLAGS $INCLUDES -c "$WORKER_DIR/task_service.grpc.pb.cc" -o task_service.grpc.pb.o

echo "[link_with_grpc] Linking final binary $OUT_BIN ..."
clang++-20 -O2 "$INPUT_LL" parallel_runtime_grpc.o task_service.pb.o task_service.grpc.pb.o $LIBS $LINK_LIBS -o "$OUT_BIN"

echo "[link_with_grpc] Done. Output: ./$OUT_BIN"



#!/bin/bash

# Build script for LLVM plugins
# Make sure LLVM is installed: sudo apt-get install llvm-14-dev clang-14

LLVM_VERSION=20  # Adjust if you have different version
LLVM_CONFIG=llvm-config-${LLVM_VERSION}

# Check if llvm-config exists
if ! command -v ${LLVM_CONFIG} &> /dev/null; then
    echo "Error: ${LLVM_CONFIG} not found"
    echo "Install LLVM: sudo apt-get install llvm-${LLVM_VERSION}-dev clang-${LLVM_VERSION}"
    exit 1
fi

# curl -X POST -F "file=@//home/niloy/vs_code/course/cse299/test1.cpp" http://localhost:8080/upload
# Get LLVM flags
CXXFLAGS=$(${LLVM_CONFIG} --cxxflags)
LDFLAGS=$(${LLVM_CONFIG} --ldflags)
LIBS=$(${LLVM_CONFIG} --libs --system-libs)

# Build PDG Pass plugin
echo "Building libPDGPass.so..."
g++ -shared -fPIC -o libPDGPass.so pdg.cpp \
    ${CXXFLAGS} \
    ${LDFLAGS} \
    ${LIBS} \
    -std=c++17

# Build Loop Outliner plugin
echo "Building libLoopOutliner.so..."
g++ -shared -fPIC -o libLoopOutliner.so parallel_loop_outline.cpp \
    ${CXXFLAGS} \
    ${LDFLAGS} \
    ${LIBS} \
    -std=c++17

echo "Done! Plugins built:"
ls -lh *.so
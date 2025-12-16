set -e

VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
VCPKG_INSTALLED="$VCPKG_ROOT/installed/x64-linux"

VCPKG_PROTOC="$VCPKG_INSTALLED/tools/protobuf/protoc"
VCPKG_GRPC_PLUGIN="$VCPKG_INSTALLED/tools/grpc/grpc_cpp_plugin"

# Clean old generated files
rm -f task_service.pb.cc task_service.pb.h task_service.grpc.pb.cc task_service.grpc.pb.h

# Regenerate with vcpkg's protoc
"$VCPKG_PROTOC" --grpc_out=. --cpp_out=. \
  --plugin=protoc-gen-grpc="$VCPKG_GRPC_PLUGIN" \
  task_service.proto
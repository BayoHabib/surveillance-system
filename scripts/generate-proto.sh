#!/bin/bash
set -e

echo "Generating Go protobuf/gRPC code..."

# Generate Go code from protobuf
protoc \
    --go_out=internal/vision/proto \
    --go_opt=paths=source_relative \
    --go-grpc_out=internal/vision/proto \
    --go-grpc_opt=paths=source_relative \
    --proto_path=internal/vision/proto \
    internal/vision/proto/vision.proto

echo "✅ Protobuf/gRPC code generated successfully!"
echo "Generated files:"
ls -la internal/vision/proto/

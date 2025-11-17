# Makefile - Vision Service C++
# Simple wrapper around CMake for easy development

# Variables
BUILD_DIR := build
BUILD_TYPE := Release
BINARY_NAME := vision-service
NUM_CORES := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Default target
.PHONY: all
all: build

# =============================================================================
# BUILD COMMANDS
# =============================================================================

# Configure CMake
.PHONY: configure
configure:
	@echo "Configuring CMake..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ..

# Build the service
.PHONY: build
build: configure
	@echo "Building vision service..."
	cd $(BUILD_DIR) && make -j$(NUM_CORES)
	@echo "✅ Build complete: $(BUILD_DIR)/$(BINARY_NAME)"

# Debug build
.PHONY: debug
debug:
	@echo "Building debug version..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(NUM_CORES)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)

# Rebuild (clean + build)
.PHONY: rebuild
rebuild: clean build

# =============================================================================
# RUN COMMANDS
# =============================================================================

# Run the service
.PHONY: run
run: build
	@echo "Starting vision service on port 50051..."
	./$(BUILD_DIR)/$(BINARY_NAME)

# Run in background
.PHONY: start
start: build
	@echo "Starting vision service in background..."
	./$(BUILD_DIR)/$(BINARY_NAME) &
	@echo "Vision service started. PID: $$!"

# Stop background service
.PHONY: stop
stop:
	@echo "Stopping vision service..."
	@pkill -f $(BINARY_NAME) || echo "No running vision service found"

# =============================================================================
# DEVELOPMENT COMMANDS
# =============================================================================

# Generate protobuf files manually (useful for IDE)
.PHONY: proto
proto:
	@echo "Generating protobuf files..."
	@mkdir -p $(BUILD_DIR)
	protoc --cpp_out=$(BUILD_DIR) --grpc_out=$(BUILD_DIR) \
		--plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
		--proto_path=proto proto/vision.proto
	@echo "Protobuf files generated in $(BUILD_DIR)/"

# Format code (requires clang-format)
.PHONY: format
format:
	@echo "Formatting C++ code..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i; \
		echo "✅ Code formatted"; \
	else \
		echo "⚠️  clang-format not found. Install with: sudo apt install clang-format"; \
	fi

# Check code style
.PHONY: lint
lint:
	@echo "Checking code style..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src tests -name "*.cpp" -o -name "*.h" | \
		xargs clang-format --dry-run --Werror 2>/dev/null && \
		echo "✅ Code style OK" || \
		echo "❌ Code style issues found. Run 'make format' to fix."; \
	else \
		echo "⚠️  clang-format not found"; \
	fi

# =============================================================================
# TESTING COMMANDS
# =============================================================================

# Build and run tests
.PHONY: test
test: build
	@echo "Running tests..."
	@if [ -f "$(BUILD_DIR)/vision-service-tests" ]; then \
		cd $(BUILD_DIR) && ./vision-service-tests; \
	else \
		echo "⚠️  Tests not built. Make sure GTest is installed."; \
		echo "Install with: sudo apt install libgtest-dev"; \
	fi

# Run tests with verbose output
.PHONY: test-verbose
test-verbose: build
	@echo "Running tests (verbose)..."
	@if [ -f "$(BUILD_DIR)/vision-service-tests" ]; then \
		cd $(BUILD_DIR) && ./vision-service-tests --gtest_output=xml:test_results.xml; \
	else \
		echo "⚠️  Tests not built"; \
	fi

# Memory check with Valgrind
.PHONY: test-memory
test-memory: debug
	@echo "Running memory checks..."
	@if command -v valgrind >/dev/null 2>&1; then \
		cd $(BUILD_DIR) && valgrind --leak-check=full --show-leak-kinds=all ./vision-service-tests; \
	else \
		echo "⚠️  Valgrind not found. Install with: sudo apt install valgrind"; \
	fi

# =============================================================================
# DEBUGGING COMMANDS
# =============================================================================

# Run with GDB
.PHONY: debug-run
debug-run: debug
	@echo "Starting GDB session..."
	cd $(BUILD_DIR) && gdb ./$(BINARY_NAME)

# Generate compile_commands.json for IDE
.PHONY: compile-db
compile-db:
	@echo "Generating compile_commands.json..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. && \
	cp compile_commands.json ..

# =============================================================================
# INSTALLATION
# =============================================================================

# Install system-wide
.PHONY: install
install: build
	@echo "Installing vision service..."
	cd $(BUILD_DIR) && sudo make install
	@echo "✅ Vision service installed to /usr/local/bin/"

# =============================================================================
# UTILITIES
# =============================================================================

# Show build info
.PHONY: info
info:
	@echo "🔧 Vision Service Build Info"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Build type: $(BUILD_TYPE)"
	@echo "Cores for parallel build: $(NUM_CORES)"
	@echo "Binary name: $(BINARY_NAME)"
	@echo ""
	@echo "System info:"
	@echo "OS: $$(uname -s)"
	@echo "Architecture: $$(uname -m)"
	@if command -v cmake >/dev/null 2>&1; then \
		echo "CMake: $$(cmake --version | head -1)"; \
	else \
		echo "CMake: ❌ Not found"; \
	fi
	@if command -v protoc >/dev/null 2>&1; then \
		echo "Protobuf: $$(protoc --version)"; \
	else \
		echo "Protobuf: ❌ Not found"; \
	fi
	@if command -v grpc_cpp_plugin >/dev/null 2>&1; then \
		echo "gRPC: ✅ Found"; \
	else \
		echo "gRPC: ❌ Not found"; \
	fi

# Check dependencies
.PHONY: deps-check
deps-check:
	@echo "🔍 Checking dependencies..."
	@echo -n "CMake: "; command -v cmake >/dev/null 2>&1 && echo "✅" || echo "❌"
	@echo -n "Protobuf: "; command -v protoc >/dev/null 2>&1 && echo "✅" || echo "❌"
	@echo -n "gRPC plugin: "; command -v grpc_cpp_plugin >/dev/null 2>&1 && echo "✅" || echo "❌"
	@echo -n "GTest: "; ldconfig -p | grep -q libgtest && echo "✅" || echo "❌"
	@echo -n "clang-format: "; command -v clang-format >/dev/null 2>&1 && echo "✅" || echo "❌ (optional)"
	@echo -n "Valgrind: "; command -v valgrind >/dev/null 2>&1 && echo "✅" || echo "❌ (optional)"

# Install dependencies (Ubuntu/Debian)
.PHONY: deps-install
deps-install:
	@echo "📦 Installing dependencies..."
	sudo apt update
	sudo apt install -y \
		build-essential \
		cmake \
		pkg-config \
		libgrpc++-dev \
		libprotobuf-dev \
		protobuf-compiler-grpc \
		libgtest-dev \
		clang-format \
		valgrind
	@echo "✅ Dependencies installed"
	
# Add this target to your Makefile
setup-gtest:
	@if [ ! -d "third_party/googletest" ]; then \
		echo "📦 Setting up GTest locally..."; \
		mkdir -p third_party; \
		git clone --depth 1 https://github.com/google/googletest.git third_party/googletest; \
		echo "✅ GTest setup complete"; \
	else \
		echo "✅ GTest already available"; \
	fi

# Update your build target
build: setup-gtest
	@echo "🔨 Building Vision Service..."
	mkdir -p build
	cd build && cmake .. && make -j$(shell nproc)

# Show help
.PHONY: help
help:
	@echo "🎥 Vision Service C++ - Available Commands"
	@echo ""
	@echo "📦 Build Commands:"
	@echo "  build         - Build the service (Release mode)"
	@echo "  debug         - Build debug version"
	@echo "  clean         - Clean build artifacts"
	@echo "  rebuild       - Clean and build"
	@echo ""
	@echo "🚀 Run Commands:"
	@echo "  run           - Build and run the service"
	@echo "  start         - Start service in background"
	@echo "  stop          - Stop background service"
	@echo ""
	@echo "🧪 Test Commands:"
	@echo "  test          - Build and run tests"
	@echo "  test-verbose  - Run tests with verbose output"
	@echo "  test-memory   - Run tests with memory checks"
	@echo ""
	@echo "🛠️  Development:"
	@echo "  format        - Format code with clang-format"
	@echo "  lint          - Check code style"
	@echo "  proto         - Generate protobuf files"
	@echo "  compile-db    - Generate compile_commands.json for IDE"
	@echo ""
	@echo "🔧 Debug Commands:"
	@echo "  debug-run     - Run with GDB debugger"
	@echo ""
	@echo "🏠 System Commands:"
	@echo "  install       - Install system-wide"
	@echo "  deps-check    - Check dependencies"
	@echo "  deps-install  - Install dependencies (Ubuntu/Debian)"
	@echo "  info          - Show build information"
	@echo "  help          - Show this help"
# =============================================================================
# OPENCV COMMANDS (Phase 2.3)
# =============================================================================

# Test OpenCV installation
.PHONY: test-opencv
test-opencv:
	@echo "🔍 Test installation OpenCV..."
	@pkg-config --exists opencv4 && echo "✅ OpenCV 4.x détecté" || \
	 pkg-config --exists opencv && echo "✅ OpenCV détecté" || \
	 echo "❌ OpenCV non trouvé"
	@echo "Version:" $$(pkg-config --modversion opencv4 2>/dev/null || pkg-config --modversion opencv 2>/dev/null)

# Build avec OpenCV
.PHONY: build-opencv
build-opencv: setup-gtest
	@echo "🔨 Building with OpenCV support..."
	mkdir -p build
	cd build && cmake -DHAVE_OPENCV=ON .. && make -j$(NUM_CORES)

# Tests OpenCV spécifiques
.PHONY: test-opencv-unit
test-opencv-unit: build-opencv
	@echo "🧪 Running OpenCV unit tests..."
	@if [ -f "build/vision-service-tests" ]; then \
		cd build && ./vision-service-tests --gtest_filter="OpenCV*"; \
	else \
		echo "⚠️ Tests not built"; \
	fi

# Debug OpenCV
.PHONY: debug-opencv
debug-opencv: build-opencv
	@echo "🔍 Debug OpenCV integration..."
	cd build && gdb ./vision-service

# Informations OpenCV
.PHONY: opencv-info
opencv-info:
	@echo "📋 OpenCV Information"
	@echo "===================="
	@pkg-config --list-all | grep opencv || echo "No OpenCV packages found"
	@echo ""
	@echo "Include directories:"
	@pkg-config --cflags opencv4 2>/dev/null || pkg-config --cflags opencv 2>/dev/null || echo "Not found"
	@echo ""
	@echo "Libraries:"
	@pkg-config --libs opencv4 2>/dev/null || pkg-config --libs opencv 2>/dev/null || echo "Not found"

# Add target for OpenCV development
.PHONY: dev-opencv
dev-opencv: clean build-opencv test-opencv
	@echo "✅ OpenCV development build complete"

# Test with OpenCV
.PHONY: test-opencv
test-opencv:
	@if [ -f "build/vision-service-tests" ]; then \
	    echo "🧪 Running OpenCV tests..."; \
	    cd build && ./vision-service-tests; \
	else \
	    echo "❌ No test executable found. Run 'make build-opencv' first."; \
	fi

# Run with OpenCV
.PHONY: run-opencv
run-opencv: build-opencv
	@echo "🚀 Starting Vision Service with OpenCV..."
	cd build && ./vision-service

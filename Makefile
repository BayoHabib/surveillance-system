# Variables
APP_NAME := surveillance-core
CPP_SERVICE_NAME := vision-service
DOCKER_IMAGE := surveillance-system
VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")

# Go parameters
GOCMD := go
GOBUILD := $(GOCMD) build
GOCLEAN := $(GOCMD) clean
GOTEST := $(GOCMD) test
GOGET := $(GOCMD) get
GOMOD := $(GOCMD) mod

# Build directories
BUILD_DIR := build
GO_BINARY := $(BUILD_DIR)/$(APP_NAME)
CPP_BINARY := vision-service/build/$(CPP_SERVICE_NAME)

# Test parameters
TEST_TIMEOUT := 30s
COVERAGE_OUT := coverage.out
COVERAGE_HTML := coverage.html

.PHONY: all build clean test deps run help

# Default target - build both services
all: clean deps test build

# =============================================================================
# DUAL SERVICE BUILD COMMANDS
# =============================================================================

# Build both Go and C++ services
build: build-go build-cpp

# Build Go service only
build-go:
	@echo "Building Go service..."
	@mkdir -p $(BUILD_DIR)
	$(GOBUILD) -o $(GO_BINARY) -v ./cmd/server

# Build C++ service only
build-cpp:
	@echo "Building C++ vision service..."
	cd vision-service && make build

# Clean both services
clean: clean-go clean-cpp

# Clean Go artifacts
clean-go:
	@echo "Cleaning Go artifacts..."
	$(GOCLEAN)
	@rm -rf $(BUILD_DIR)
	@rm -f $(COVERAGE_OUT) $(COVERAGE_HTML)
	@rm -f *.prof
	@rm -f test.db

# Clean C++ artifacts
clean-cpp:
	@echo "Cleaning C++ artifacts..."
	cd vision-service && make clean

# Download dependencies for both services
deps: deps-go deps-cpp

# Go dependencies
deps-go:
	@echo "Downloading Go dependencies..."
	$(GOMOD) download
	$(GOMOD) tidy

# C++ dependencies check
deps-cpp:
	@echo "Checking C++ dependencies..."
	cd vision-service && make deps-check

# Install C++ dependencies (Ubuntu/Debian)
deps-cpp-install:
	@echo "Installing C++ dependencies..."
	cd vision-service && make deps-install

# =============================================================================
# DUAL SERVICE RUN COMMANDS
# =============================================================================

# Run both services (requires 2 terminals)
run-all:
	@echo "🚀 Starting both services..."
	@echo "This will start both Go and C++ services."
	@echo "Go service: http://localhost:8080"
	@echo "C++ service: gRPC on localhost:50051"
	@echo ""
	@echo "Starting C++ service in background..."
	cd vision-service && make start
	@sleep 2
	@echo "Starting Go service..."
	make run-go

# Run Go service only
run-go: build-go
	@echo "Starting Go service..."
	./$(GO_BINARY)

# Run C++ service only
run-cpp: build-cpp
	@echo "Starting C++ vision service..."
	cd vision-service && make run

# Start both services in background
start-all:
	@echo "Starting both services in background..."
	cd vision-service && make start
	@sleep 1
	nohup ./$(GO_BINARY) > go-service.log 2>&1 &
	@echo "Services started:"
	@echo "- Go service: http://localhost:8080 (PID: $$!)"
	@echo "- C++ service: gRPC on localhost:50051"
	@echo "Logs: go-service.log and vision-service logs"

# Stop both background services
stop-all:
	@echo "Stopping all services..."
	cd vision-service && make stop
	@pkill -f $(APP_NAME) || echo "Go service not running"
	@rm -f go-service.log

# Run with hot reload (Go only)
dev: build-cpp
	@echo "Starting development mode..."
	@echo "C++ service must be running. Starting it now..."
	cd vision-service && make start
	@echo "Starting Go service with hot reload..."
	@if ! command -v air > /dev/null; then \
		echo "Installing air for hot reload..."; \
		go install github.com/cosmtrek/air@latest; \
	fi
	air

# =============================================================================
# DUAL SERVICE TESTING
# =============================================================================

# Test both services
test: test-go test-cpp

# Test Go service only
test-go:
	@echo "Running Go tests..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) ./internal/...

# Test C++ service only
test-cpp:
	@echo "Running C++ tests..."
	cd vision-service && make test

# Integration tests (both services)
test-integration: build
	@echo "Running integration tests..."
	@echo "Starting C++ service for integration tests..."
	cd vision-service && make start
	@sleep 2
	@echo "Running Go integration tests..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -tags=integration ./...
	@echo "Stopping C++ service..."
	cd vision-service && make stop

# Test with coverage (Go)
test-coverage:
	@echo "Running Go tests with coverage..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -coverprofile=$(COVERAGE_OUT) ./internal/...
	go tool cover -html=$(COVERAGE_OUT) -o $(COVERAGE_HTML)
	@echo "Coverage report generated: $(COVERAGE_HTML)"
	@echo "Open with: open $(COVERAGE_HTML)"

# Test with race detection (Go)
test-race:
	@echo "Running Go tests with race detection..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -race ./internal/...

# Benchmarks for both services
test-bench: test-bench-go test-bench-cpp

test-bench-go:
	@echo "Running Go benchmarks..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -bench=. -benchmem ./internal/...

test-bench-cpp:
	@echo "Running C++ benchmarks..."
	cd vision-service && make test-bench

# =============================================================================
# CODE QUALITY
# =============================================================================

# Lint both services
lint: lint-go lint-cpp

# Format and lint Go code
lint-go: fmt vet

# Lint C++ code
lint-cpp:
	@echo "Linting C++ code..."
	cd vision-service && make lint

# Format Go code
fmt:
	@echo "Formatting Go code..."
	go fmt ./...
	@echo "Checking Go formatting..."
	@if [ -n "$$(gofmt -l .)" ]; then \
		echo "The following files are not formatted:"; \
		gofmt -l .; \
		exit 1; \
	fi

# Go vet
vet:
	@echo "Running go vet..."
	go vet ./...

# Format C++ code
format-cpp:
	@echo "Formatting C++ code..."
	cd vision-service && make format

# Complete validation for both services
validate: clean deps lint test-race test-coverage build
	@echo "✅ Complete validation passed for both services!"

# =============================================================================
# SERVICE MANAGEMENT
# =============================================================================

# Check status of both services
status: status-go status-cpp

status-go:
	@echo "📋 Go Service Status:"
	@echo "Git branch: $$(git branch --show-current 2>/dev/null || echo 'Not a git repo')"
	@if [ -f "$(GO_BINARY)" ]; then \
		echo "✅ Go binary exists: $(GO_BINARY)"; \
		ls -lh $(GO_BINARY); \
	else \
		echo "❌ Go binary not built"; \
	fi
	@if pgrep -f $(APP_NAME) > /dev/null; then \
		echo "✅ Go service is running (PID: $$(pgrep -f $(APP_NAME)))"; \
	else \
		echo "❌ Go service not running"; \
	fi

status-cpp:
	@echo "📋 C++ Service Status:"
	cd vision-service && make info
	@if [ -f "$(CPP_BINARY)" ]; then \
		echo "✅ C++ binary exists: $(CPP_BINARY)"; \
	else \
		echo "❌ C++ binary not built"; \
	fi
	@if pgrep -f $(CPP_SERVICE_NAME) > /dev/null; then \
		echo "✅ C++ service is running (PID: $$(pgrep -f $(CPP_SERVICE_NAME)))"; \
	else \
		echo "❌ C++ service not running"; \
	fi

# Health check both services
health-check:
	@echo "🏥 Health Check - Both Services"
	@echo ""
	@echo "Go Service (HTTP):"
	@curl -s http://localhost:8080/api/v1/health | jq . || echo "❌ Go service not responding"
	@echo ""
	@echo "C++ Service (gRPC):"
	@grpcurl -plaintext localhost:50051 surveillance.vision.VisionService/GetHealth || echo "❌ C++ service not responding"

# =============================================================================
# DOCKER COMMANDS (Updated for dual service)
# =============================================================================

# Build Docker image with both services
docker-build:
	@echo "Building Docker image with both services..."
	docker build -t $(DOCKER_IMAGE):$(VERSION) .
	docker tag $(DOCKER_IMAGE):$(VERSION) $(DOCKER_IMAGE):latest

# Run Docker container
docker-run:
	@echo "Running Docker container..."
	docker run -d \
		--name surveillance-system \
		-p 8080:8080 \
		-p 50051:50051 \
		$(DOCKER_IMAGE):latest

# Stop Docker container
docker-stop:
	@echo "Stopping Docker container..."
	docker stop surveillance-system || true
	docker rm surveillance-system || true

# Docker Compose commands
compose-up:
	@echo "Starting services with Docker Compose..."
	docker-compose up -d

compose-down:
	@echo "Stopping services..."
	docker-compose down

compose-logs:
	@echo "Showing logs..."
	docker-compose logs -f

# =============================================================================
# DEVELOPMENT SETUP
# =============================================================================

# Setup development environment for both services
setup-dev:
	@echo "Setting up development environment..."
	@mkdir -p web logs build vision-service/build
	@echo "Installing Go development tools..."
	@if ! command -v air > /dev/null; then \
		echo "Installing air for hot reload..."; \
		go install github.com/cosmtrek/air@latest; \
	fi
	@echo "Checking C++ dependencies..."
	cd vision-service && make deps-check
	@echo ""
	@echo "Development environment ready!"
	@echo ""
	@echo "Quick start:"
	@echo "  make build        # Build both services"
	@echo "  make test         # Test both services"
	@echo "  make dev          # Start development mode"
	@echo "  make run-all      # Run both services"

# =============================================================================
# UTILITIES
# =============================================================================

# Show metrics for both services
metrics:
	@echo "📊 Project Metrics:"
	@echo ""
	@echo "Go Service:"
	@echo "  Lines of code:"
	@find . -name "*.go" -not -path "./vendor/*" -not -path "./build/*" -not -path "./vision-service/*" | xargs wc -l | tail -1
	@echo "  Go files:"
	@find . -name "*.go" -not -path "./vendor/*" -not -path "./build/*" -not -path "./vision-service/*" | wc -l
	@echo ""
	@echo "C++ Service:"
	@echo "  Lines of code:"
	@find vision-service -name "*.cpp" -o -name "*.h" | xargs wc -l | tail -1
	@echo "  C++ files:"
	@find vision-service -name "*.cpp" -o -name "*.h" | wc -l
	@echo ""
	@echo "Total packages:"
	@go list ./... | wc -l

# Generate documentation
docs:
	@echo "📚 Generating documentation..."
	@echo "Go documentation:"
	godoc -http=:6060 &
	@echo "Go docs available at: http://localhost:6060"
	@echo ""
	@echo "C++ documentation would require Doxygen (not implemented yet)"

# =============================================================================
# HELP
# =============================================================================

# Show help
help:
	@echo "🎥 Surveillance System - Dual Service Commands"
	@echo ""
	@echo "📦 Build Commands:"
	@echo "  build         - Build both Go and C++ services"
	@echo "  build-go      - Build Go service only"
	@echo "  build-cpp     - Build C++ service only"
	@echo "  clean         - Clean both services"
	@echo "  deps          - Download dependencies for both"
	@echo ""
	@echo "🚀 Run Commands:"
	@echo "  run-all       - Run both services (requires 2 terminals)"
	@echo "  run-go        - Run Go service only"
	@echo "  run-cpp       - Run C++ service only"
	@echo "  start-all     - Start both services in background"
	@echo "  stop-all      - Stop both background services"
	@echo "  dev           - Development mode (C++ + Go hot reload)"
	@echo ""
	@echo "🧪 Testing Commands:"
	@echo "  test          - Test both services"
	@echo "  test-go       - Test Go service only"
	@echo "  test-cpp      - Test C++ service only"
	@echo "  test-integration - Integration tests (both services)"
	@echo "  test-coverage - Go tests with coverage"
	@echo "  test-race     - Go tests with race detection"
	@echo ""
	@echo "✨ Quality Commands:"
	@echo "  lint          - Lint both services"
	@echo "  lint-go       - Lint Go code (fmt + vet)"
	@echo "  lint-cpp      - Lint C++ code"
	@echo "  format-cpp    - Format C++ code"
	@echo "  validate      - Complete validation pipeline"
	@echo ""
	@echo "🔧 Service Management:"
	@echo "  status        - Check status of both services"
	@echo "  health-check  - Health check both services"
	@echo "  metrics       - Show code metrics"
	@echo ""
	@echo "🐳 Docker Commands:"
	@echo "  docker-build  - Build Docker image (both services)"
	@echo "  docker-run    - Run Docker container"
	@echo "  compose-up    - Start with Docker Compose"
	@echo ""
	@echo "🛠️  Setup Commands:"
	@echo "  setup-dev     - Setup development environment"
	@echo "  deps-cpp-install - Install C++ dependencies"
	@echo ""
	@echo "🎯 Quick Development Workflow:"
	@echo "  1. make setup-dev     # First time setup"
	@echo "  2. make build         # Build both services"
	@echo "  3. make test          # Run all tests"
	@echo "  4. make dev           # Start development"
	@echo ""
	@echo "📡 Service Endpoints:"
	@echo "  Go Service:  http://localhost:8080 (REST + WebSocket)"
	@echo "  C++ Service: localhost:50051 (gRPC)"
	@echo "  Dashboard:   http://localhost:8080"

# Legacy commands for compatibility
run: run-go
	@echo "⚠️  Note: 'make run' now runs Go service only."
	@echo "   Use 'make run-all' to run both services."
	@echo "   Use 'make dev' for development mode."
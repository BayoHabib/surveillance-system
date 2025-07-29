# Variables
APP_NAME := surveillance-core
DOCKER_IMAGE := surveillance-system
VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")

# Go parameters
GOCMD := go
GOBUILD := $(GOCMD) build
GOCLEAN := $(GOCMD) clean
GOTEST := $(GOCMD) test
GOGET := $(GOCMD) get
GOMOD := $(GOCMD) mod

# Build directory
BUILD_DIR := build
BINARY_NAME := $(BUILD_DIR)/$(APP_NAME)

# Test parameters
TEST_TIMEOUT := 30s
COVERAGE_OUT := coverage.out
COVERAGE_HTML := coverage.html

.PHONY: all build clean test deps run help

# Default target
all: clean deps test build

# =============================================================================
# LOCAL DEVELOPMENT COMMANDS
# =============================================================================

# Build the application
build:
	@echo "Building $(APP_NAME)..."
	@mkdir -p $(BUILD_DIR)
	$(GOBUILD) -o $(BINARY_NAME) -v ./cmd/server

# Clean build artifacts
clean:
	@echo "Cleaning..."
	$(GOCLEAN)
	@rm -rf $(BUILD_DIR)
	@rm -f $(COVERAGE_OUT) $(COVERAGE_HTML)
	@rm -f *.prof
	@rm -f test.db

# Download dependencies
deps:
	@echo "Downloading dependencies..."
	$(GOMOD) download
	$(GOMOD) tidy

# Run the application locally
run: build
	@echo "Starting $(APP_NAME)..."
	./$(BINARY_NAME)

# Run with hot reload (requires air: go install github.com/cosmtrek/air@latest)
dev:
	@echo "Starting development server with hot reload..."
	@if ! command -v air > /dev/null; then \
		echo "Installing air for hot reload..."; \
		go install github.com/cosmtrek/air@latest; \
	fi
	air

# =============================================================================
# TESTING COMMANDS (LOCAL)
# =============================================================================

# Run all tests
test: test-unit

# Tests unitaires uniquement
test-unit:
	@echo "Running unit tests..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) ./internal/...

# Tests d'intégration (à implémenter plus tard)
test-integration:
	@echo "Running integration tests..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -tags=integration ./...

# Tests avec couverture de code
test-coverage:
	@echo "Running tests with coverage..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -coverprofile=$(COVERAGE_OUT) ./internal/...
	go tool cover -html=$(COVERAGE_OUT) -o $(COVERAGE_HTML)
	@echo "Coverage report generated: $(COVERAGE_HTML)"
	@echo "Open with: open $(COVERAGE_HTML)"

# Tests de race conditions
test-race:
	@echo "Running tests with race detection..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -race ./internal/...

# Benchmarks
test-bench:
	@echo "Running benchmarks..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -bench=. -benchmem ./internal/...

# Tests en mode verbose
test-verbose:
	@echo "Running verbose tests..."
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) -count=1 ./internal/...

# Test d'un package spécifique
test-package:
	@echo "Usage: make test-package PKG=internal/core"
	@if [ -z "$(PKG)" ]; then \
		echo "Error: PKG variable is required"; \
		echo "Example: make test-package PKG=internal/core"; \
		exit 1; \
	fi
	$(GOTEST) -v -timeout $(TEST_TIMEOUT) ./$(PKG)/...

# =============================================================================
# CODE QUALITY (LOCAL)
# =============================================================================

# Lint complet
lint: fmt vet

# Formatage du code
fmt:
	@echo "Formatting code..."
	go fmt ./...
	@echo "Checking formatting..."
	@if [ -n "$$(gofmt -l .)" ]; then \
		echo "The following files are not formatted:"; \
		gofmt -l .; \
		exit 1; \
	fi

# Analyse statique Go
vet:
	@echo "Running go vet..."
	go vet ./...

# Analyse statique avancée (optionnelle)
staticcheck:
	@echo "Running staticcheck..."
	@if ! command -v staticcheck > /dev/null; then \
		echo "staticcheck not installed. Install with:"; \
		echo "go install honnef.co/go/tools/cmd/staticcheck@latest"; \
		exit 1; \
	fi
	staticcheck ./...

# Validation complète (pour développement local)
validate: clean deps fmt vet test-race test-coverage
	@echo "✅ Local validation complete!"

# =============================================================================
# DOCKER COMMANDS (OPTIONNELLES)
# =============================================================================

# Docker commands (utilise seulement si tu veux tester en container)
docker-build:
	@echo "Building Docker image..."
	docker build -t $(DOCKER_IMAGE):$(VERSION) .
	docker tag $(DOCKER_IMAGE):$(VERSION) $(DOCKER_IMAGE):latest

docker-run:
	@echo "Running Docker container..."
	docker run -d \
		--name $(APP_NAME) \
		-p 8080:8080 \
		$(DOCKER_IMAGE):latest

docker-stop:
	@echo "Stopping Docker container..."
	docker stop $(APP_NAME) || true
	docker rm $(APP_NAME) || true

# Docker Compose commands (utilise seulement pour déploiement)
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

# Setup development environment
setup-dev:
	@echo "Setting up development environment..."
	@mkdir -p web logs build
	@echo "Installing development tools..."
	@if ! command -v air > /dev/null; then \
		echo "Installing air for hot reload..."; \
		go install github.com/cosmtrek/air@latest; \
	fi
	@echo "Development environment ready!"
	@echo ""
	@echo "Quick start:"
	@echo "  make dev    # Start with hot reload"
	@echo "  make test   # Run tests"
	@echo "  make lint   # Check code quality"

# Install development tools (optionnel)
install-tools:
	@echo "Installing development tools..."
	go install github.com/cosmtrek/air@latest
	@echo "Optional tools (run manually if needed):"
	@echo "  go install honnef.co/go/tools/cmd/staticcheck@latest"
	@echo "  go install github.com/fzipp/gocyclo/cmd/gocyclo@latest"

# =============================================================================
# PROJECT STRUCTURE
# =============================================================================

# Initialize project structure
init-project:
	@echo "Initializing project structure..."
	@mkdir -p cmd/server
	@mkdir -p internal/{api,core,vision,websocket}
	@mkdir -p pkg/{types,utils}
	@mkdir -p web/static
	@mkdir -p logs
	@mkdir -p build
	@echo "Project structure created!"

# =============================================================================
# UTILITIES
# =============================================================================

# Show code metrics
metrics:
	@echo "📊 Code metrics:"
	@echo "Lines of code:"
	@find . -name "*.go" -not -path "./vendor/*" -not -path "./build/*" | xargs wc -l | tail -1
	@echo "Number of files:"
	@find . -name "*.go" -not -path "./vendor/*" -not -path "./build/*" | wc -l
	@echo "Number of packages:"
	@go list ./... | wc -l

# Show git status and info
status:
	@echo "📋 Project status:"
	@echo "Git branch: $$(git branch --show-current 2>/dev/null || echo 'Not a git repo')"
	@echo "Git status:"
	@git status --porcelain 2>/dev/null || echo "Not a git repository"
	@echo ""
	@echo "Build status:"
	@if [ -f "$(BINARY_NAME)" ]; then \
		echo "✅ Binary exists: $(BINARY_NAME)"; \
		ls -lh $(BINARY_NAME); \
	else \
		echo "❌ Binary not built"; \
	fi

# Clean everything including Docker
clean-all: clean docker-stop
	@echo "Cleaning Docker images..."
	@docker rmi $(DOCKER_IMAGE):latest $(DOCKER_IMAGE):$(VERSION) 2>/dev/null || true
	@echo "Cleaning Docker volumes..."
	@docker volume prune -f 2>/dev/null || true

# =============================================================================
# HELP
# =============================================================================

# Show help
help:
	@echo "🎥 Surveillance System - Available Commands"
	@echo ""
	@echo "📦 Local Development:"
	@echo "  build         - Build the application"
	@echo "  clean         - Clean build artifacts"
	@echo "  run           - Build and run the application"
	@echo "  dev           - Run with hot reload (requires air)"
	@echo "  deps          - Download dependencies"
	@echo ""
	@echo "🧪 Testing:"
	@echo "  test          - Run unit tests"
	@echo "  test-coverage - Run tests with coverage report"
	@echo "  test-race     - Run tests with race detection"
	@echo "  test-bench    - Run benchmarks"
	@echo "  test-package PKG=path - Test specific package"
	@echo ""
	@echo "✨ Code Quality:"
	@echo "  lint          - Run linting (fmt + vet)"
	@echo "  fmt           - Format code"
	@echo "  vet           - Run go vet"
	@echo "  validate      - Run complete validation"
	@echo ""
	@echo "🐳 Docker (Optional):"
	@echo "  docker-build  - Build Docker image"
	@echo "  docker-run    - Run Docker container"
	@echo "  docker-stop   - Stop Docker container"
	@echo "  compose-up    - Start with Docker Compose"
	@echo "  compose-down  - Stop Docker Compose services"
	@echo ""
	@echo "🛠️  Setup:"
	@echo "  setup-dev     - Setup development environment"
	@echo "  install-tools - Install optional development tools"
	@echo ""
	@echo "📊 Info:"
	@echo "  metrics       - Show code metrics"
	@echo "  status        - Show project status"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "🚀 Quick Start:"
	@echo "  make setup-dev  # First time setup"
	@echo "  make test       # Run tests"
	@echo "  make dev        # Start development server"
// internal/vision/client.go
package vision

import (
	"log"
	"os"
	//"surveillance-core/internal/core"
)

// ClientType represents the type of vision client to create
type ClientType string

const (
	ClientTypeMock ClientType = "mock"
	ClientTypeGRPC ClientType = "grpc"
)

// ClientConfig holds configuration for vision clients
type ClientConfig struct {
	Type        ClientType `json:"type"`
	GRPCAddress string     `json:"grpc_address"`
}

// DefaultClientConfig returns default configuration
func DefaultClientConfig() *ClientConfig {
	return &ClientConfig{
		Type:        ClientTypeMock, // Default to mock for backward compatibility
		GRPCAddress: "localhost:50051",
	}
}

// NewClient creates a vision client based on configuration
func NewClient(config *ClientConfig) Client {
	if config == nil {
		config = DefaultClientConfig()
	}

	// Override with environment variables if set
	if envType := os.Getenv("VISION_CLIENT_TYPE"); envType != "" {
		config.Type = ClientType(envType)
	}

	if envAddr := os.Getenv("VISION_SERVICE_ADDRESS"); envAddr != "" {
		config.GRPCAddress = envAddr
	}

	log.Printf("🔧 Creating vision client: type=%s, address=%s", config.Type, config.GRPCAddress)

	switch config.Type {
	case ClientTypeGRPC:
		return NewGRPCClient(config.GRPCAddress)
	case ClientTypeMock:
		return NewMockClient()
	default:
		log.Printf("⚠️ Unknown client type %s, falling back to mock", config.Type)
		return NewMockClient()
	}
}

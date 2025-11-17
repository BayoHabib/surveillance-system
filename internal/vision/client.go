// internal/vision/client.go
package vision

import (
	"log"
	"os"
)

// ClientType represents the type of vision client to create
type ClientType string

const (
	ClientTypeMock ClientType = "mock"
	ClientTypeGRPC ClientType = "grpc"
)

// ClientConfig holds configuration for vision clients
type ClientConfig struct {
	Type        ClientType
	GRPCAddress string
	Timeout     int // seconds
}

// DefaultClientConfig returns default configuration based on environment
func DefaultClientConfig() *ClientConfig {
	clientType := os.Getenv("VISION_CLIENT_TYPE")
	if clientType == "" {
		clientType = "grpc" // Default to gRPC for integration
	}

	address := os.Getenv("VISION_SERVICE_ADDRESS")
	if address == "" {
		address = "localhost:50051"
	}

	config := &ClientConfig{
		Type:        ClientType(clientType),
		GRPCAddress: address,
		Timeout:     30,
	}

	log.Printf("🔧 Creating vision client: type=%s, address=%s", config.Type, config.GRPCAddress)
	return config
}

// NewClient creates a vision client based on configuration
func NewClient(config *ClientConfig) VisionClient {
	if config == nil {
		config = DefaultClientConfig()
	}

	switch config.Type {
	case ClientTypeGRPC:
		return NewGRPCClient(config)
	case ClientTypeMock:
		return NewMockClient()
	default:
		log.Printf("⚠️  Unknown client type %s, using mock", config.Type)
		return NewMockClient()
	}
}

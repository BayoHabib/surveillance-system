package vision

import (
	"surveillance-core/internal/core"
	pb "surveillance-core/internal/vision/proto"
)

// Client defines the interface for a vision client, abstracting the underlying
// implementation (e.g., gRPC, mock).
type Client interface {
	// StartStream begins a standard video stream.
	StartStream(cameraID string) (<-chan core.Frame, error)
	// StartStreamWithURL begins a stream from an internet URL.
	StartStreamWithURL(cameraID string, cameraURL string) (<-chan core.Frame, error)
	// StopStream terminates a video stream.
	StopStream(cameraID string) error
	// GetStream retrieves the frame channel for an existing stream.
	GetStream(cameraID string) (<-chan core.Frame, error)
	// GetStreamStatus returns the current status of a stream.
	GetStreamStatus(cameraID string) core.StreamStatus
	// IsConnected checks the connection status to the vision service.
	IsConnected() bool
	// HealthCheck performs a health check on the vision service.
	HealthCheck() error
}

// GRPCClientProvider provides access to the underlying gRPC client
type GRPCClientProvider interface {
	GetGRPCClient() pb.VisionServiceClient
}

// VisionClient is an alias for Client for backward compatibility.
type VisionClient = Client

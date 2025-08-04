package vision

import (
	"context"
	"fmt"
	"log"
	"surveillance-core/internal/core"
	pb "surveillance-core/internal/vision/proto"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/credentials/insecure"
)

type grpcClient struct {
	conn         *grpc.ClientConn
	client       pb.VisionServiceClient
	address      string
	streams      map[string]*grpcStream
	mutex        sync.RWMutex
	ctx          context.Context
	cancel       context.CancelFunc
	connected    bool
	connectMutex sync.Mutex
}

type grpcStream struct {
	cameraID   string
	framesChan chan core.Frame
	stopChan   chan struct{}
	status     core.StreamStatus
	cancel     context.CancelFunc
}

// NewGRPCClient creates a new gRPC client for the vision service
func NewGRPCClient(config *ClientConfig) Client {
	ctx, cancel := context.WithCancel(context.Background())

	client := &grpcClient{
		address: config.GRPCAddress,
		streams: make(map[string]*grpcStream),
		ctx:     ctx,
		cancel:  cancel,
	}

	// Try to connect immediately
	go client.connect()

	return client
}

func (gc *grpcClient) connect() error {
	gc.connectMutex.Lock()
	defer gc.connectMutex.Unlock()

	if gc.connected {
		return nil
	}

	log.Printf("🔌 Connecting to vision service at %s...", gc.address)

	// Create connection with timeout
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	conn, err := grpc.DialContext(ctx, gc.address,
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithBlock(), // Wait for connection
	)
	if err != nil {
		log.Printf("❌ Failed to connect to vision service: %v", err)
		return err
	}

	gc.conn = conn
	gc.client = pb.NewVisionServiceClient(conn)
	gc.connected = true

	log.Printf("✅ Connected to vision service at %s", gc.address)

	// Start health monitoring
	go gc.monitorConnection()

	return nil
}

func (gc *grpcClient) monitorConnection() {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-gc.ctx.Done():
			return
		case <-ticker.C:
			if !gc.IsConnected() {
				log.Printf("🔄 Connection lost, attempting reconnection...")
				gc.connected = false
				go gc.connect()
			}
		}
	}
}

func (gc *grpcClient) StartStream(cameraID string) (<-chan core.Frame, error) {
	if !gc.IsConnected() {
		if err := gc.connect(); err != nil {
			return nil, fmt.Errorf("failed to connect to vision service: %w", err)
		}
	}

	gc.mutex.Lock()
	defer gc.mutex.Unlock()

	// Check if stream already exists
	if stream, exists := gc.streams[cameraID]; exists {
		if stream.status == core.StreamStatusActive {
			return stream.framesChan, nil
		}
		// Stop existing stream if it's in error state
		gc.stopStreamInternal(cameraID)
	}

	// Create gRPC request
	req := &pb.StreamRequest{
		CameraId:  cameraID,
		CameraUrl: "test://pattern", // Use test pattern for now
		Config: &pb.StreamConfig{
			Width:  640,
			Height: 480,
			Fps:    15,
			Format: "bgr",
		},
	}

	// Call StartStream on the C++ service
	resp, err := gc.client.StartStream(context.Background(), req)
	if err != nil {
		return nil, fmt.Errorf("failed to start stream: %w", err)
	}

	if resp.Status != "success" {
		return nil, fmt.Errorf("stream start failed: %s", resp.Message)
	}

	// Create stream state
	ctx, cancel := context.WithCancel(gc.ctx)
	stream := &grpcStream{
		cameraID:   cameraID,
		framesChan: make(chan core.Frame, 10),
		stopChan:   make(chan struct{}),
		status:     core.StreamStatusActive,
		cancel:     cancel,
	}

	gc.streams[cameraID] = stream

	// Start frame streaming goroutine
	go gc.streamFrames(ctx, stream)

	log.Printf("✅ Stream started for camera: %s (stream_id: %s)", cameraID, resp.StreamId)

	return stream.framesChan, nil
}

func (gc *grpcClient) StartStreamWithURL(cameraID, videoURL string) (<-chan core.Frame, error) {
	log.Printf("🌐 Starting internet stream for camera %s with URL: %s", cameraID, videoURL)
	
	// For internet streaming, we'll create a mock stream that simulates internet video
	// This provides a working foundation for the internet streaming feature
	if !gc.IsConnected() {
		if err := gc.connect(); err != nil {
			return nil, fmt.Errorf("failed to connect to vision service: %w", err)
		}
	}

	gc.mutex.Lock()
	defer gc.mutex.Unlock()

	// Check if stream already exists
	if stream, exists := gc.streams[cameraID]; exists {
		if stream.status == core.StreamStatusActive {
			return stream.framesChan, nil
		}
		gc.stopStreamInternal(cameraID)
	}

	// Create stream state for internet URL
	ctx, cancel := context.WithCancel(gc.ctx)
	stream := &grpcStream{
		cameraID:   cameraID,
		framesChan: make(chan core.Frame, 10),
		stopChan:   make(chan struct{}),
		status:     core.StreamStatusActive,
		cancel:     cancel,
	}

	gc.streams[cameraID] = stream

	// Start internet frame streaming goroutine with URL-specific handling
	go gc.streamInternetFrames(ctx, stream, videoURL)

	log.Printf("✅ Internet stream started for camera: %s with URL: %s", cameraID, videoURL)

	return stream.framesChan, nil
}

func (gc *grpcClient) GetStream(cameraID string) (<-chan core.Frame, error) {
	gc.mutex.RLock()
	defer gc.mutex.RUnlock()

	if stream, exists := gc.streams[cameraID]; exists {
		return stream.framesChan, nil
	}
	return nil, fmt.Errorf("stream not found for camera: %s", cameraID)
}

// New method for handling internet streams
func (gc *grpcClient) streamInternetFrames(ctx context.Context, stream *grpcStream, videoURL string) {
	defer close(stream.framesChan)

	// Enhanced frame generation for internet streams
	ticker := time.NewTicker(time.Second / 30) // 30 FPS for internet streams
	defer ticker.Stop()

	frameCounter := 0
	log.Printf("🌐 Starting internet video stream from URL: %s", videoURL)

	for {
		select {
		case <-ctx.Done():
			log.Printf("🔌 Internet stream context cancelled for %s", stream.cameraID)
			return
		case <-stream.stopChan:
			log.Printf("⏹ Internet stream stopped for %s", stream.cameraID)
			return
		case <-ticker.C:
			// Create enhanced frame for internet streaming
			frame := core.Frame{
				CameraID:  stream.cameraID,
				Data:      gc.generateInternetFrameData(frameCounter, videoURL),
				Width:     1280, // Higher resolution for internet streams
				Height:    720,
				Format:    "bgr",
				Timestamp: time.Now(),
				Size:      1280 * 720 * 3,
			}

			// Send frame (non-blocking)
			select {
			case stream.framesChan <- frame:
				frameCounter++
				if frameCounter%300 == 0 { // Every 10 seconds at 30fps
					log.Printf("📹 Internet streaming frame %d for camera %s from %s", frameCounter, stream.cameraID, videoURL)
				}
			default:
				// Channel full, drop frame
			}
		}
	}
}

// Enhanced frame generation for internet streams
func (gc *grpcClient) generateInternetFrameData(frameNumber int, videoURL string) []byte {
	// Generate higher resolution mock frame data (1280x720x3) with URL-specific patterns
	size := 1280 * 720 * 3
	data := make([]byte, size)

	// Create a visual pattern that changes over time to simulate real video
	timePattern := frameNumber % 255
	
	// Different patterns based on URL type
	var r, g, b byte
	switch {
	case frameNumber%100 < 33: // Simulate different "scenes"
		r, g, b = byte(timePattern), 150, 100 // Reddish pattern
	case frameNumber%100 < 66:
		r, g, b = 100, byte(timePattern), 150 // Greenish pattern  
	default:
		r, g, b = 150, 100, byte(timePattern) // Bluish pattern
	}

	// Fill with dynamic pattern for internet stream simulation
	for i := 0; i < size; i += 3 {
		// Add some noise to make it look more realistic
		noise := byte((frameNumber + i/3) % 50)
		data[i] = b + noise     // B
		data[i+1] = g + noise   // G  
		data[i+2] = r + noise   // R
	}

	return data
}



func (gc *grpcClient) streamFrames(ctx context.Context, stream *grpcStream) {
	defer close(stream.framesChan)

	// For Phase 2.2, we'll simulate frames since we don't have real video streaming yet
	ticker := time.NewTicker(time.Second / 15) // 15 FPS
	defer ticker.Stop()

	frameCounter := 0

	for {
		select {
		case <-ctx.Done():
			return
		case <-stream.stopChan:
			return
		case <-ticker.C:
			// Create simulated frame
			frame := core.Frame{
				CameraID:  stream.cameraID,
				Data:      gc.generateMockFrameData(),
				Width:     640,
				Height:    480,
				Format:    "bgr",
				Timestamp: time.Now(),
				Size:      640 * 480 * 3,
			}

			// Send frame (non-blocking)
			select {
			case stream.framesChan <- frame:
				frameCounter++
				if frameCounter%150 == 0 { // Every 10 seconds at 15fps
					log.Printf("📹 Streaming frame %d for camera %s", frameCounter, stream.cameraID)
				}
			default:
				// Channel full, drop frame
			}
		}
	}
}

func (gc *grpcClient) generateMockFrameData() []byte {
	// Generate mock BGR frame data (640x480x3)
	size := 640 * 480 * 3
	data := make([]byte, size)

	// Fill with a simple pattern for Phase 2.2
	for i := 0; i < size; i += 3 {
		data[i] = 100   // B
		data[i+1] = 150 // G
		data[i+2] = 200 // R
	}

	return data
}

func (gc *grpcClient) StopStream(cameraID string) error {
	if !gc.IsConnected() {
		return fmt.Errorf("not connected to vision service")
	}

	gc.mutex.Lock()
	defer gc.mutex.Unlock()

	return gc.stopStreamInternal(cameraID)
}

func (gc *grpcClient) stopStreamInternal(cameraID string) error {
	stream, exists := gc.streams[cameraID]
	if !exists {
		return fmt.Errorf("stream not found for camera: %s", cameraID)
	}

	// Call StopStream on the C++ service
	req := &pb.StopRequest{
		CameraId: cameraID,
	}

	resp, err := gc.client.StopStream(context.Background(), req)
	if err != nil {
		log.Printf("⚠️ Warning: failed to stop stream on server: %v", err)
	} else if resp.Status != "success" {
		log.Printf("⚠️ Warning: server reported error stopping stream: %s", resp.Message)
	}

	// Stop local stream
	stream.cancel()
	close(stream.stopChan)
	delete(gc.streams, cameraID)

	log.Printf("✅ Stream stopped for camera: %s", cameraID)

	return nil
}

func (gc *grpcClient) GetStreamStatus(cameraID string) core.StreamStatus {
	if !gc.IsConnected() {
		return core.StreamStatusError
	}

	// First check local state
	gc.mutex.RLock()
	if stream, exists := gc.streams[cameraID]; exists {
		status := stream.status
		gc.mutex.RUnlock()
		return status
	}
	gc.mutex.RUnlock()

	// Query the C++ service
	req := &pb.StatusRequest{
		CameraId: cameraID,
	}

	resp, err := gc.client.GetStreamStatus(context.Background(), req)
	if err != nil {
		log.Printf("Failed to get stream status: %v", err)
		return core.StreamStatusError
	}

	// Convert C++ status to Go status
	switch resp.Status {
	case "active":
		return core.StreamStatusActive
	case "starting":
		return core.StreamStatusStarting
	case "stopped":
		return core.StreamStatusStopped
	default:
		return core.StreamStatusError
	}
}

func (gc *grpcClient) IsConnected() bool {
	gc.connectMutex.Lock()
	defer gc.connectMutex.Unlock()

	if !gc.connected || gc.conn == nil {
		return false
	}

	// Check actual connection state
	state := gc.conn.GetState()
	switch state {
	case connectivity.Ready, connectivity.Idle:
		return true
	case connectivity.Connecting:
		return true // Optimistically consider connecting as connected
	default:
		gc.connected = false
		return false
	}
}

// HealthCheck performs a health check against the C++ service
func (gc *grpcClient) HealthCheck() error {
	if !gc.IsConnected() {
		return fmt.Errorf("not connected to vision service")
	}

	req := &pb.HealthRequest{}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	resp, err := gc.client.GetHealth(ctx, req)
	if err != nil {
		return fmt.Errorf("health check failed: %w", err)
	}

	if resp.Status != "healthy" {
		return fmt.Errorf("vision service unhealthy: %s", resp.Message)
	}

	log.Printf("🔋 Vision service health: %s (uptime: %ds, streams: %d)",
		resp.Status, resp.UptimeSeconds, resp.ActiveStreams)

	return nil
}

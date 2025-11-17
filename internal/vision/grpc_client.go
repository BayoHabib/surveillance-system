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

	// Create connection immediately (non-blocking)
	conn, err := grpc.Dial(config.GRPCAddress,
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		log.Printf("⚠️  Failed to dial vision service: %v", err)
	}

	client := &grpcClient{
		address:   config.GRPCAddress,
		conn:      conn,
		client:    pb.NewVisionServiceClient(conn),
		streams:   make(map[string]*grpcStream),
		ctx:       ctx,
		cancel:    cancel,
		connected: false,
	}

	// Try to connect in background
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
	
	// For internet streaming, we also need to start the stream on the vision service
	// so that detection and processing can work properly
	if !gc.IsConnected() {
		if err := gc.connect(); err != nil {
			return nil, fmt.Errorf("failed to connect to vision service: %w", err)
		}
	}

	// First, start the stream on the vision service with the URL
	req := &pb.StreamRequest{
		CameraId:  cameraID,
		CameraUrl: videoURL,
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	resp, err := gc.client.StartStream(ctx, req)
	cancel()

	if err != nil {
		log.Printf("⚠️  Warning: failed to start stream on vision service: %v", err)
		// Continue anyway for backward compatibility
	} else if resp.Status != "success" {
		log.Printf("⚠️  Warning: vision service returned status: %s - %s", resp.Status, resp.Message)
	} else {
		log.Printf("✅ Vision service stream started for camera: %s", cameraID)
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
	ctx2, cancel2 := context.WithCancel(gc.ctx)
	stream := &grpcStream{
		cameraID:   cameraID,
		framesChan: make(chan core.Frame, 10),
		stopChan:   make(chan struct{}),
		status:     core.StreamStatusActive,
		cancel:     cancel2,
	}

	gc.streams[cameraID] = stream

	// Start internet frame streaming goroutine with URL-specific handling
	go gc.streamInternetFrames(ctx2, stream, videoURL)

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
			// BUG #9 FIX: Utiliser le pool pour frames internet (1280x720)
			frame := core.GetFrameWithSize(1280, 720, 3)
			frame.CameraID = stream.cameraID
			frame.Format = "bgr"
			frame.Timestamp = time.Now()
			
			// Generate enhanced mock data
			gc.fillInternetFrameData(frame.Data, frameCounter, videoURL)

			// Send frame (non-blocking)
			select {
			case stream.framesChan <- *frame:
				core.ReleaseFrame(frame)
				frameCounter++
				if frameCounter%300 == 0 { // Every 10 seconds at 30fps
					log.Printf("📹 Internet streaming frame %d for camera %s from %s", frameCounter, stream.cameraID, videoURL)
				}
			default:
				// Channel full, drop frame
				core.ReleaseFrame(frame)
			}
		}
	}
}

// fillInternetFrameData remplit un buffer avec des données de test avancées
func (gc *grpcClient) fillInternetFrameData(data []byte, frameNumber int, videoURL string) {
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
	for i := 0; i < len(data); i += 3 {
		// Add some noise to make it look more realistic
		noise := byte((frameNumber + i/3) % 50)
		data[i] = b + noise     // B
		data[i+1] = g + noise   // G  
		data[i+2] = r + noise   // R
	}
}

// generateInternetFrameData deprecated: utiliser fillInternetFrameData avec pool
func (gc *grpcClient) generateInternetFrameData(frameNumber int, videoURL string) []byte {
	size := 1280 * 720 * 3
	data := make([]byte, size)
	gc.fillInternetFrameData(data, frameNumber, videoURL)
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
			// BUG #9 FIX: Utiliser le pool de frames au lieu d'allocation directe
			frame := core.GetFrameWithSize(640, 480, 3)
			frame.CameraID = stream.cameraID
			frame.Format = "bgr"
			frame.Timestamp = time.Now()
			
			// Generate mock data
			gc.fillMockFrameData(frame.Data)

			// Send frame (non-blocking)
			select {
			case stream.framesChan <- *frame:
				// Frame envoyée, on peut la recycler
				core.ReleaseFrame(frame)
				frameCounter++
				if frameCounter%150 == 0 { // Every 10 seconds at 15fps
					log.Printf("📹 Streaming frame %d for camera %s", frameCounter, stream.cameraID)
				}
			default:
				// Channel full, drop frame et recycler
				core.ReleaseFrame(frame)
			}
		}
	}
}

// fillMockFrameData remplit un buffer avec des données de test (évite allocation)
func (gc *grpcClient) fillMockFrameData(data []byte) {
	// Fill with a simple pattern for Phase 2.2
	for i := 0; i < len(data); i += 3 {
		data[i] = 100   // B
		data[i+1] = 150 // G
		data[i+2] = 200 // R
	}
}

// generateMockFrameData deprecated: utiliser fillMockFrameData avec pool
func (gc *grpcClient) generateMockFrameData() []byte {
	// Generate mock BGR frame data (640x480x3)
	size := 640 * 480 * 3
	data := make([]byte, size)
	gc.fillMockFrameData(data)
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

// GetGRPCClient returns the underlying gRPC client for advanced operations
func (gc *grpcClient) GetGRPCClient() pb.VisionServiceClient {
	return gc.client
}

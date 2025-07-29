// internal/vision/mock_test.go
package vision

import (
	"surveillance-core/internal/core"
	"testing"
	"time"
)

func TestMockClient_StartStream(t *testing.T) {
	client := NewMockClient()

	// Test démarrage stream
	framesChan, err := client.StartStream("test_cam")
	if err != nil {
		t.Errorf("Unexpected error starting stream: %v", err)
	}
	if framesChan == nil {
		t.Errorf("Expected frames channel, got nil")
	}

	// Vérifier le statut
	status := client.GetStreamStatus("test_cam")
	if status != StreamStatusActive {
		t.Errorf("Expected status %s, got %s", StreamStatusActive, status)
	}

	// Nettoyer
	client.StopStream("test_cam")
}

func TestMockClient_StartStream_AlreadyRunning(t *testing.T) {
	client := NewMockClient()

	// Démarrer le stream
	framesChan1, err := client.StartStream("test_cam")
	if err != nil {
		t.Fatalf("Unexpected error starting first stream: %v", err)
	}

	// Démarrer le même stream à nouveau
	framesChan2, err := client.StartStream("test_cam")
	if err != nil {
		t.Errorf("Unexpected error starting second stream: %v", err)
	}

	// Devrait retourner le même channel
	if framesChan1 != framesChan2 {
		t.Errorf("Expected same channel for same camera")
	}

	// Nettoyer
	client.StopStream("test_cam")
}

func TestMockClient_StopStream(t *testing.T) {
	client := NewMockClient()

	// Démarrer puis arrêter
	_, err := client.StartStream("test_cam")
	if err != nil {
		t.Fatalf("Unexpected error starting stream: %v", err)
	}

	err = client.StopStream("test_cam")
	if err != nil {
		t.Errorf("Unexpected error stopping stream: %v", err)
	}

	// Vérifier le statut
	status := client.GetStreamStatus("test_cam")
	if status != StreamStatusStopped {
		t.Errorf("Expected status %s, got %s", StreamStatusStopped, status)
	}
}

func TestMockClient_StopStream_NotRunning(t *testing.T) {
	client := NewMockClient()

	// Arrêter un stream qui n'existe pas
	err := client.StopStream("nonexistent_cam")
	if err == nil {
		t.Errorf("Expected error stopping nonexistent stream")
	}
}

func TestMockClient_GetStreamStatus(t *testing.T) {
	client := NewMockClient()

	// Statut initial
	status := client.GetStreamStatus("test_cam")
	if status != StreamStatusStopped {
		t.Errorf("Expected initial status %s, got %s", StreamStatusStopped, status)
	}

	// Après démarrage
	client.StartStream("test_cam")
	status = client.GetStreamStatus("test_cam")
	if status != StreamStatusActive {
		t.Errorf("Expected active status %s, got %s", StreamStatusActive, status)
	}

	// Nettoyer
	client.StopStream("test_cam")
}

func TestMockClient_IsConnected(t *testing.T) {
	client := NewMockClient()

	// Mock devrait toujours être connecté
	if !client.IsConnected() {
		t.Errorf("Mock client should always be connected")
	}
}

func TestMockClient_FrameGeneration(t *testing.T) {
	client := NewMockClient()

	framesChan, err := client.StartStream("test_cam")
	if err != nil {
		t.Fatalf("Unexpected error starting stream: %v", err)
	}

	// Attendre quelques frames
	framesReceived := 0
	timeout := time.After(2 * time.Second)

	for framesReceived < 3 {
		select {
		case frame := <-framesChan:
			framesReceived++

			// Vérifier les propriétés de la frame
			if frame.CameraID != "test_cam" {
				t.Errorf("Expected camera ID 'test_cam', got '%s'", frame.CameraID)
			}
			if frame.Width != 1920 {
				t.Errorf("Expected width 1920, got %d", frame.Width)
			}
			if frame.Height != 1080 {
				t.Errorf("Expected height 1080, got %d", frame.Height)
			}
			if frame.Format != "jpeg" {
				t.Errorf("Expected format 'jpeg', got '%s'", frame.Format)
			}
			if len(frame.Data) == 0 {
				t.Errorf("Expected frame data, got empty")
			}
			if frame.Size != len(frame.Data) {
				t.Errorf("Expected size %d, got %d", len(frame.Data), frame.Size)
			}

			// Vérifier que le timestamp est récent
			if time.Since(frame.Timestamp) > time.Second {
				t.Errorf("Frame timestamp should be recent")
			}

		case <-timeout:
			t.Fatalf("Timeout waiting for frames, only received %d", framesReceived)
		}
	}

	// Nettoyer
	client.StopStream("test_cam")
}

func TestMockClient_MultipleStreams(t *testing.T) {
	client := NewMockClient()

	// Démarrer plusieurs streams
	cameras := []string{"cam_001", "cam_002", "cam_003"}
	channels := make(map[string]<-chan core.Frame)

	for _, camID := range cameras {
		framesChan, err := client.StartStream(camID)
		if err != nil {
			t.Errorf("Error starting stream for %s: %v", camID, err)
		}
		channels[camID] = framesChan
	}

	// Vérifier que tous les streams sont actifs
	for _, camID := range cameras {
		status := client.GetStreamStatus(camID)
		if status != StreamStatusActive {
			t.Errorf("Expected %s to be active, got %s", camID, status)
		}
	}

	// Recevoir quelques frames de chaque stream
	framesCounts := make(map[string]int)
	timeout := time.After(3 * time.Second)
	targetFrames := 2

	for {
		allCamerasGotFrames := true
		for _, camID := range cameras {
			if framesCounts[camID] < targetFrames {
				allCamerasGotFrames = false
				break
			}
		}

		if allCamerasGotFrames {
			break
		}

		select {
		case frame := <-channels["cam_001"]:
			framesCounts["cam_001"]++
			if frame.CameraID != "cam_001" {
				t.Errorf("Wrong camera ID in frame")
			}
		case frame := <-channels["cam_002"]:
			framesCounts["cam_002"]++
			if frame.CameraID != "cam_002" {
				t.Errorf("Wrong camera ID in frame")
			}
		case frame := <-channels["cam_003"]:
			framesCounts["cam_003"]++
			if frame.CameraID != "cam_003" {
				t.Errorf("Wrong camera ID in frame")
			}
		case <-timeout:
			t.Fatalf("Timeout waiting for frames from all cameras")
		}
	}

	// Nettoyer tous les streams
	for _, camID := range cameras {
		client.StopStream(camID)
	}
}

func TestMockClient_FrameDataVariation(t *testing.T) {
	client := NewMockClient()

	framesChan, err := client.StartStream("test_cam")
	if err != nil {
		t.Fatalf("Unexpected error starting stream: %v", err)
	}

	// Collecter plusieurs frames pour vérifier la variation
	frames := make([]core.Frame, 0, 5)
	timeout := time.After(2 * time.Second)

	for len(frames) < 5 {
		select {
		case frame := <-framesChan:
			frames = append(frames, frame)
		case <-timeout:
			t.Fatalf("Timeout collecting frames")
		}
	}

	// Vérifier que les tailles varient (simulation réaliste)
	sizes := make(map[int]bool)
	for _, frame := range frames {
		sizes[frame.Size] = true

		// Vérifier que la taille est dans une plage raisonnable
		if frame.Size < 65536 || frame.Size > 98304 { // Entre 64KB et 96KB
			t.Errorf("Frame size %d outside expected range", frame.Size)
		}
	}

	// Devrait y avoir au moins quelques tailles différentes
	if len(sizes) < 2 {
		t.Errorf("Expected some variation in frame sizes, got only %d different sizes", len(sizes))
	}

	// Nettoyer
	client.StopStream("test_cam")
}

// Benchmark pour les performances
func BenchmarkMockClient_FrameGeneration(b *testing.B) {
	client := NewMockClient()

	framesChan, err := client.StartStream("bench_cam")
	if err != nil {
		b.Fatalf("Error starting stream: %v", err)
	}

	b.ResetTimer()

	framesReceived := 0
	timeout := time.After(5 * time.Second)

	for framesReceived < b.N {
		select {
		case <-framesChan:
			framesReceived++
		case <-timeout:
			b.Fatalf("Timeout during benchmark")
		}
	}

	client.StopStream("bench_cam")
}

func TestMockClient_ChannelCleanup(t *testing.T) {
	client := NewMockClient()

	framesChan, err := client.StartStream("test_cam")
	if err != nil {
		t.Fatalf("Error starting stream: %v", err)
	}

	// Arrêter le stream
	err = client.StopStream("test_cam")
	if err != nil {
		t.Errorf("Error stopping stream: %v", err)
	}

	// Vérifier que le channel est fermé
	timeout := time.After(1 * time.Second)
	select {
	case _, ok := <-framesChan:
		if ok {
			t.Errorf("Channel should be closed after stopping stream")
		}
	case <-timeout:
		t.Errorf("Channel should be closed immediately after stopping")
	}
}

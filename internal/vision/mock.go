package vision

import (
	"fmt"
	"math/rand"
	"surveillance-core/internal/core"
	"sync"
	"time"

	"github.com/google/uuid"
)

type mockClient struct {
	streams map[string]*mockStream
	mutex   sync.RWMutex
}

type mockStream struct {
	cameraID   string
	status     core.StreamStatus
	framesChan chan core.Frame
	stopChan   chan bool
	ticker     *time.Ticker
}

func NewMockClient() Client {
	return &mockClient{
		streams: make(map[string]*mockStream),
	}
}

func (mc *mockClient) StartStream(cameraID string) (<-chan core.Frame, error) {
	mc.mutex.Lock()
	defer mc.mutex.Unlock()

	if stream, exists := mc.streams[cameraID]; exists {
		// Return existing stream if already running
		return stream.framesChan, nil
	}

	// Créer un nouveau flux mock
	stream := &mockStream{
		cameraID:   cameraID,
		status:     core.StreamStatusStarting,
		framesChan: make(chan core.Frame, 10), // Tampon de 10 frames
		stopChan:   make(chan bool),
		ticker:     time.NewTicker(100 * time.Millisecond), // 10 FPS
	}

	mc.streams[cameraID] = stream
	fmt.Printf("Stream démarré pour caméra: %s\n", cameraID)

	go mc.generateFrames(stream)

	// Attendre que le flux soit prêt
	time.Sleep(50 * time.Millisecond)

	return stream.framesChan, nil
}

func (mc *mockClient) GetStream(cameraID string) (<-chan core.Frame, error) {
	mc.mutex.RLock()
	defer mc.mutex.RUnlock()

	if stream, exists := mc.streams[cameraID]; exists {
		return stream.framesChan, nil
	}
	return nil, fmt.Errorf("stream not found for camera: %s", cameraID)
}

func (mc *mockClient) HealthCheck() error {
	// Mock is always healthy
	return nil
}

func (mc *mockClient) StartStreamWithURL(cameraID string, cameraURL string) (<-chan core.Frame, error) {
	fmt.Printf("Mock: StartStreamWithURL - Camera: %s, URL: %s\n", cameraID, cameraURL)
	// For mock, just call the regular StartStream method
	return mc.StartStream(cameraID)
}

func (mc *mockClient) StopStream(cameraID string) error {
	mc.mutex.Lock()
	defer mc.mutex.Unlock()

	return mc.stopStreamInternal(cameraID)
}

func (mc *mockClient) stopStreamInternal(cameraID string) error {
	stream, exists := mc.streams[cameraID]
	if !exists {
		return fmt.Errorf("stream non trouvé pour caméra: %s", cameraID)
	}

	// Arrêter le générateur
	stream.ticker.Stop()
	close(stream.stopChan)
	close(stream.framesChan)

	delete(mc.streams, cameraID)
	fmt.Printf("Stream arrêté pour caméra: %s\n", cameraID)

	return nil
}

func (mc *mockClient) GetStreamStatus(cameraID string) core.StreamStatus {
	mc.mutex.RLock()
	defer mc.mutex.RUnlock()

	if stream, exists := mc.streams[cameraID]; exists {
		return stream.status
	}
	return core.StreamStatusStopped
}

func (mc *mockClient) IsConnected() bool {
	return true // Mock toujours connecté
}

func (mc *mockClient) generateFrames(stream *mockStream) {
	// Set status to active once frame generation starts
	mc.mutex.Lock()
	stream.status = core.StreamStatusActive
	mc.mutex.Unlock()
	
	detectionCounter := 0

	for {
		select {
		case <-stream.stopChan:
			return

		case <-stream.ticker.C:
			// Générer frame mock
			imageData := mc.generateMockImageData()
			frame := core.Frame{
				CameraID:  stream.cameraID,
				Data:      imageData,
				Width:     1920,
				Height:    1080,
				Format:    "jpeg",
				Timestamp: time.Now(),
				Size:      len(imageData), // ← CORRECTION: utiliser la vraie taille
			}

			// Envoyer frame (non-bloquant)
			select {
			case stream.framesChan <- frame:
				// Simuler détections occasionnelles
				detectionCounter++
				if detectionCounter%45 == 0 { // Toutes les 3 secondes à 15fps
					mc.simulateDetection(stream.cameraID)
				}
			default:
				// Canal plein, ignorer cette frame
			}
		}
	}
}

func (mc *mockClient) generateMockImageData() []byte {
	// Génère des données d'image factices
	size := 65536 + rand.Intn(32768) // Entre 64KB et 96KB
	data := make([]byte, size)

	// Remplir avec du bruit pseudo-aléatoire pour simuler une image JPEG
	for i := range data {
		data[i] = byte(rand.Intn(256))
	}

	return data
}

func (mc *mockClient) simulateDetection(cameraID string) {
	// Simuler différents types de détections
	detectionTypes := []core.DetectionType{
		core.DetectionTypeMotion,
		core.DetectionTypePerson,
		core.DetectionTypeVehicle,
	}

	detectionType := detectionTypes[rand.Intn(len(detectionTypes))]
	confidence := 0.6 + rand.Float32()*0.4 // Entre 0.6 et 1.0

	detection := core.Detection{
		ID:         uuid.New().String(),
		CameraID:   cameraID,
		Type:       detectionType,
		Confidence: confidence,
		BBox: core.BoundingBox{
			X:      rand.Intn(1600),
			Y:      rand.Intn(900),
			Width:  100 + rand.Intn(300),
			Height: 100 + rand.Intn(300),
		},
		Timestamp: time.Now(),
		Metadata: map[string]string{
			"source":        "mock_detector",
			"model_version": "1.0.0",
		},
	}

	fmt.Printf("🔍 Détection simulée: %s sur %s (confiance: %.2f)\n",
		detectionType, cameraID, confidence)

	// Dans un vrai système, ceci irait vers l'EventProcessor
	// Pour le mock, on utilise la variable pour éviter l'erreur de compilation
	_ = detection // Utilisation explicite pour éviter l'erreur "declared and not used"
}

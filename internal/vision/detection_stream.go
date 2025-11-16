// internal/vision/detection_stream.go
package vision

import (
	"context"
	"fmt"
	"io"
	"log"
	"sync"
	"time"

	"surveillance-core/internal/core"
	pb "surveillance-core/internal/vision/proto"
)

// DetectionStreamManager gère les streams de détection pour toutes les caméras
type DetectionStreamManager struct {
	client        pb.VisionServiceClient
	alertManager  core.AlertManager
	activeStreams map[string]context.CancelFunc
	mutex         sync.RWMutex
}

// NewDetectionStreamManager crée un nouveau gestionnaire de streams
func NewDetectionStreamManager(client pb.VisionServiceClient, alertManager core.AlertManager) *DetectionStreamManager {
	return &DetectionStreamManager{
		client:        client,
		alertManager:  alertManager,
		activeStreams: make(map[string]context.CancelFunc),
	}
}

// StartDetectionStream démarre le streaming de détections pour une caméra
func (dsm *DetectionStreamManager) StartDetectionStream(cameraID string) error {
	dsm.mutex.Lock()
	
	// Vérifier si un stream existe déjà
	if cancel, exists := dsm.activeStreams[cameraID]; exists {
		dsm.mutex.Unlock()
		log.Printf("[DetectionStream] Stream already active for camera: %s", cameraID)
		// Arrêter l'ancien avant de redémarrer
		cancel()
		time.Sleep(100 * time.Millisecond)
		dsm.mutex.Lock()
	}
	
	// Créer un contexte annulable
	ctx, cancel := context.WithCancel(context.Background())
	dsm.activeStreams[cameraID] = cancel
	dsm.mutex.Unlock()
	
	// Démarrer le stream dans une goroutine
	go dsm.streamDetections(ctx, cameraID)
	
	log.Printf("[DetectionStream] ✅ Started detection stream for camera: %s", cameraID)
	return nil
}

// StopDetectionStream arrête le streaming pour une caméra
func (dsm *DetectionStreamManager) StopDetectionStream(cameraID string) {
	dsm.mutex.Lock()
	defer dsm.mutex.Unlock()
	
	if cancel, exists := dsm.activeStreams[cameraID]; exists {
		cancel()
		delete(dsm.activeStreams, cameraID)
		log.Printf("[DetectionStream] ⏹️  Stopped detection stream for camera: %s", cameraID)
	}
}

// StopAll arrête tous les streams actifs
func (dsm *DetectionStreamManager) StopAll() {
	dsm.mutex.Lock()
	defer dsm.mutex.Unlock()
	
	for cameraID, cancel := range dsm.activeStreams {
		cancel()
		log.Printf("[DetectionStream] Stopped stream for camera: %s", cameraID)
	}
	dsm.activeStreams = make(map[string]context.CancelFunc)
}

// GetActiveStreams retourne la liste des caméras avec stream actif
func (dsm *DetectionStreamManager) GetActiveStreams() []string {
	dsm.mutex.RLock()
	defer dsm.mutex.RUnlock()
	
	cameras := make([]string, 0, len(dsm.activeStreams))
	for cameraID := range dsm.activeStreams {
		cameras = append(cameras, cameraID)
	}
	return cameras
}

// streamDetections gère le stream de détections pour une caméra
func (dsm *DetectionStreamManager) streamDetections(ctx context.Context, cameraID string) {
	log.Printf("[DetectionStream] Starting detection listener for camera: %s", cameraID)
	
	// Créer la requête de stream
	req := &pb.DetectionStreamRequest{
		CameraId:      cameraID,
		IncludeFrames: false, // Pas besoin des frames pour les alertes
	}
	
	// Démarrer le stream gRPC
	stream, err := dsm.client.StreamDetections(ctx, req)
	if err != nil {
		log.Printf("[DetectionStream] ❌ Failed to start stream for %s: %v", cameraID, err)
		dsm.StopDetectionStream(cameraID)
		return
	}
	
	log.Printf("[DetectionStream] 📡 Stream established for camera: %s", cameraID)
	
	// Compteurs pour statistiques
	detectionCount := 0
	alertCount := 0
	startTime := time.Now()
	
	// Boucle de réception des événements
	for {
		select {
		case <-ctx.Done():
			log.Printf("[DetectionStream] Context cancelled for camera: %s", cameraID)
			return
		default:
			// Recevoir un événement
			event, err := stream.Recv()
			if err == io.EOF {
				log.Printf("[DetectionStream] Stream ended for camera: %s", cameraID)
				dsm.StopDetectionStream(cameraID)
				return
			}
			if err != nil {
				log.Printf("[DetectionStream] ❌ Error receiving event for %s: %v", cameraID, err)
				dsm.StopDetectionStream(cameraID)
				return
			}
			
			// Traiter l'événement de détection
			detectionCount++
			if dsm.processDetectionEvent(event) {
				alertCount++
			}
			
			// Log périodique des stats (toutes les 100 détections)
			if detectionCount%100 == 0 {
				elapsed := time.Since(startTime).Seconds()
				rate := float64(detectionCount) / elapsed
				log.Printf("[DetectionStream] 📊 Camera %s: %d detections, %d alerts, %.1f det/s", 
					cameraID, detectionCount, alertCount, rate)
			}
		}
	}
}

// processDetectionEvent traite un événement de détection et crée une alerte si nécessaire
func (dsm *DetectionStreamManager) processDetectionEvent(event *pb.DetectionEvent) bool {
	if event == nil || event.Detection == nil {
		return false
	}
	
	// Log de l'événement
	log.Printf("[DetectionStream] 🎯 Detection: camera=%s, type=%s, confidence=%.2f, pixels=%d, frame=%d",
		event.CameraId,
		event.Detection.Type,
		event.Detection.Confidence,
		event.MotionPixels,
		event.FrameNumber)
	
	// Créer une alerte à partir de la détection
	alert := dsm.createAlertFromDetection(event)
	
	// Ajouter l'alerte au manager
	dsm.alertManager.AddAlert(alert)
	
	log.Printf("[DetectionStream] 🚨 Alert created: id=%s, type=%s, level=%s",
		alert.ID, alert.Type, alert.Level)
	
	return true
}

// createAlertFromDetection convertit un événement de détection en alerte
func (dsm *DetectionStreamManager) createAlertFromDetection(event *pb.DetectionEvent) core.Alert {
	detection := event.Detection
	
	// Déterminer le type d'alerte
	alertType := core.AlertTypeMotion
	if detection.Type == "person" {
		alertType = core.AlertTypeIntrusion
	}
	
	// Déterminer le niveau basé sur la confiance et les pixels
	alertLevel := core.AlertLevelInfo
	if event.MotionPixels > 5000 || detection.Confidence > 0.8 {
		alertLevel = core.AlertLevelWarning
	}
	if event.MotionPixels > 10000 || detection.Confidence > 0.9 {
		alertLevel = core.AlertLevelCritical
	}
	
	// Créer le message d'alerte
	message := fmt.Sprintf("Motion detected with %d pixels (confidence: %.2f%%)", 
		event.MotionPixels, detection.Confidence*100)
	
	// Convertir la détection proto en type core
	coreDetection := &core.Detection{
		ID:         detection.Id,
		CameraID:   event.CameraId,
		Type:       core.DetectionType(detection.Type),
		Confidence: detection.Confidence,
		Timestamp:  time.Unix(0, detection.Timestamp*1000000), // Convertir ms en nanosecondes
		Metadata: map[string]string{
			"frame_number":  fmt.Sprintf("%d", event.FrameNumber),
			"motion_pixels": fmt.Sprintf("%d", event.MotionPixels),
		},
	}
	
	// Ajouter le bounding box si disponible
	if detection.Bbox != nil {
		coreDetection.BBox = core.BoundingBox{
			X:      int(detection.Bbox.X),
			Y:      int(detection.Bbox.Y),
			Width:  int(detection.Bbox.Width),
			Height: int(detection.Bbox.Height),
		}
	}
	
	// Créer l'alerte
	return core.Alert{
		ID:        fmt.Sprintf("alert_%s_%d", event.CameraId, time.Now().UnixNano()),
		CameraID:  event.CameraId,
		Type:      alertType,
		Level:     alertLevel,
		Message:   message,
		Detection: coreDetection,
		Timestamp: time.Now(),
		Metadata: map[string]string{
			"detection_id": event.DetectionId,
			"source":       "opencv_mog2",
		},
	}
}

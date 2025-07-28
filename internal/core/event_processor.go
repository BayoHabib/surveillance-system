// internal/core/event_processor.go
package core

import (
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/google/uuid"
)

type EventProcessor interface {
	ProcessDetection(detection Detection) []Alert
	RegisterDetector(name string, detector Detector)
	SetAlertCallback(callback func(Alert))
	GetStats() ProcessorStats
}

type Detector interface {
	ShouldAlert(detection Detection) bool
	CreateAlert(detection Detection) Alert
}

type ProcessorStats struct {
	TotalDetections int64     `json:"total_detections"`
	TotalAlerts     int64     `json:"total_alerts"`
	LastProcessed   time.Time `json:"last_processed"`
	ProcessingRate  float64   `json:"processing_rate"` // détections/seconde
}

type eventProcessor struct {
	detectors     map[string]Detector
	alertCallback func(Alert)
	stats         ProcessorStats
	mutex         sync.RWMutex
	
	// Métriques de performance
	lastMinuteDetections []time.Time
}

func NewEventProcessor() EventProcessor {
	ep := &eventProcessor{
		detectors:            make(map[string]Detector),
		lastMinuteDetections: make([]time.Time, 0),
	}
	
	// Enregistrement des détecteurs par défaut
	ep.RegisterDetector("motion", &MotionDetector{})
	ep.RegisterDetector("intrusion", &IntrusionDetector{})
	
	// Nettoyage périodique des métriques
	go ep.cleanupMetrics()
	
	return ep
}

func (ep *eventProcessor) ProcessDetection(detection Detection) []Alert {
	ep.mutex.Lock()
	defer ep.mutex.Unlock()
	
	// Mise à jour des statistiques
	ep.stats.TotalDetections++
	ep.stats.LastProcessed = time.Now()
	ep.lastMinuteDetections = append(ep.lastMinuteDetections, time.Now())
	
	var alerts []Alert
	
	// Test de chaque détecteur
	for name, detector := range ep.detectors {
		if detector.ShouldAlert(detection) {
			alert := detector.CreateAlert(detection)
			alert.ID = uuid.New().String()
			alert.Timestamp = time.Now()
			
			alerts = append(alerts, alert)
			ep.stats.TotalAlerts++
			
			log.Printf("Alert générée par %s: %s", name, alert.Message)
			
			// Callback vers WebSocket
			if ep.alertCallback != nil {
				ep.alertCallback(alert)
			}
		}
	}
	
	return alerts
}

func (ep *eventProcessor) RegisterDetector(name string, detector Detector) {
	ep.mutex.Lock()
	defer ep.mutex.Unlock()
	
	ep.detectors[name] = detector
	log.Printf("Détecteur enregistré: %s", name)
}

func (ep *eventProcessor) SetAlertCallback(callback func(Alert)) {
	ep.mutex.Lock()
	defer ep.mutex.Unlock()
	
	ep.alertCallback = callback
}

func (ep *eventProcessor) GetStats() ProcessorStats {
	ep.mutex.RLock()
	defer ep.mutex.RUnlock()
	
	// Calcul du taux de traitement
	now := time.Now()
	cutoff := now.Add(-time.Minute)
	recentDetections := 0
	
	for _, t := range ep.lastMinuteDetections {
		if t.After(cutoff) {
			recentDetections++
		}
	}
	
	stats := ep.stats
	stats.ProcessingRate = float64(recentDetections) / 60.0 // détections/seconde
	
	return stats
}

func (ep *eventProcessor) cleanupMetrics() {
	ticker := time.NewTicker(time.Minute)
	defer ticker.Stop()
	
	for range ticker.C {
		ep.mutex.Lock()
		cutoff := time.Now().Add(-time.Minute)
		filtered := make([]time.Time, 0)
		
		for _, t := range ep.lastMinuteDetections {
			if t.After(cutoff) {
				filtered = append(filtered, t)
			}
		}
		
		ep.lastMinuteDetections = filtered
		ep.mutex.Unlock()
	}
}

// Détecteurs concrets

type MotionDetector struct{}

func (md *MotionDetector) ShouldAlert(detection Detection) bool {
	return detection.Type == DetectionTypeMotion && detection.Confidence > 0.7
}

func (md *MotionDetector) CreateAlert(detection Detection) Alert {
	return Alert{
		CameraID:  detection.CameraID,
		Type:      AlertTypeMotion,
		Level:     AlertLevelInfo,
		Message:   fmt.Sprintf("Mouvement détecté (confiance: %.2f)", detection.Confidence),
		Detection: &detection,
	}
}

type IntrusionDetector struct{}

func (id *IntrusionDetector) ShouldAlert(detection Detection) bool {
	return detection.Type == DetectionTypeIntrusion || 
		   (detection.Type == DetectionTypePerson && detection.Confidence > 0.8)
}

func (id *IntrusionDetector) CreateAlert(detection Detection) Alert {
	level := AlertLevelWarning
	if detection.Type == DetectionTypeIntrusion {
		level = AlertLevelCritical
	}
	
	return Alert{
		CameraID:  detection.CameraID,
		Type:      AlertTypeIntrusion,
		Level:     level,
		Message:   fmt.Sprintf("Intrusion possible détectée (confiance: %.2f)", detection.Confidence),
		Detection: &detection,
	}
}

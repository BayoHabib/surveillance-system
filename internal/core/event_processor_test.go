// internal/core/event_processor_test.go
package core

import (
	"testing"
	"time"

	"github.com/google/uuid"
)

// Mock detector pour les tests
type mockDetector struct {
	shouldAlert bool
	alertLevel  AlertLevel
}

func (md *mockDetector) ShouldAlert(detection Detection) bool {
	return md.shouldAlert
}

func (md *mockDetector) CreateAlert(detection Detection) Alert {
	return Alert{
		CameraID:  detection.CameraID,
		Type:      AlertTypeMotion,
		Level:     md.alertLevel,
		Message:   "Test alert",
		Detection: &detection,
	}
}

func TestEventProcessor_ProcessDetection(t *testing.T) {
	tests := []struct {
		name           string
		detection      Detection
		detectorSetup  func(ep EventProcessor)
		expectedAlerts int
	}{
		{
			name: "Should create alert when detector triggers",
			detection: Detection{
				ID:         uuid.New().String(),
				CameraID:   "test_cam",
				Type:       DetectionTypeMotion,
				Confidence: 0.8,
				Timestamp:  time.Now(),
			},
			detectorSetup: func(ep EventProcessor) {
				ep.RegisterDetector("test", &mockDetector{
					shouldAlert: true,
					alertLevel:  AlertLevelWarning,
				})
			},
			expectedAlerts: 1,
		},
		{
			name: "Should not create alert when detector doesn't trigger",
			detection: Detection{
				ID:         uuid.New().String(),
				CameraID:   "test_cam",
				Type:       DetectionTypeMotion,
				Confidence: 0.5,
				Timestamp:  time.Now(),
			},
			detectorSetup: func(ep EventProcessor) {
				ep.RegisterDetector("test", &mockDetector{
					shouldAlert: false,
					alertLevel:  AlertLevelInfo,
				})
			},
			expectedAlerts: 0,
		},
		{
			name: "Should handle multiple detectors",
			detection: Detection{
				ID:         uuid.New().String(),
				CameraID:   "test_cam",
				Type:       DetectionTypeMotion,
				Confidence: 0.9,
				Timestamp:  time.Now(),
			},
			detectorSetup: func(ep EventProcessor) {
				ep.RegisterDetector("detector1", &mockDetector{shouldAlert: true, alertLevel: AlertLevelWarning})
				ep.RegisterDetector("detector2", &mockDetector{shouldAlert: true, alertLevel: AlertLevelCritical})
				ep.RegisterDetector("detector3", &mockDetector{shouldAlert: false, alertLevel: AlertLevelInfo})
			},
			expectedAlerts: 2,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Créer un processor vide pour chaque test (sans détecteurs par défaut)
			ep := NewEventProcessorEmpty()

			// Setup des détecteurs
			if tt.detectorSetup != nil {
				tt.detectorSetup(ep)
			}

			// Traiter la détection
			alerts := ep.ProcessDetection(tt.detection)

			// Vérifier le nombre d'alertes
			if len(alerts) != tt.expectedAlerts {
				t.Errorf("Expected %d alerts, got %d", tt.expectedAlerts, len(alerts))
			}

			// Vérifier les stats
			stats := ep.GetStats()
			if stats.TotalDetections != 1 {
				t.Errorf("Expected 1 total detection, got %d", stats.TotalDetections)
			}
			if stats.TotalAlerts != int64(len(alerts)) {
				t.Errorf("Expected %d total alerts, got %d", len(alerts), stats.TotalAlerts)
			}
		})
	}
}

func TestEventProcessor_AlertCallback(t *testing.T) {
	ep := NewEventProcessorEmpty()

	// Setup callback pour capturer les alertes
	var capturedAlerts []Alert
	ep.SetAlertCallback(func(alert Alert) {
		capturedAlerts = append(capturedAlerts, alert)
	})

	// Ajouter un détecteur qui trigger
	ep.RegisterDetector("test", &mockDetector{shouldAlert: true, alertLevel: AlertLevelCritical})

	// Traiter une détection
	detection := Detection{
		ID:         uuid.New().String(),
		CameraID:   "test_cam",
		Type:       DetectionTypeMotion,
		Confidence: 0.8,
		Timestamp:  time.Now(),
	}

	alerts := ep.ProcessDetection(detection)

	// Vérifier que le callback a été appelé
	if len(capturedAlerts) != len(alerts) {
		t.Errorf("Expected callback to be called %d times, got %d", len(alerts), len(capturedAlerts))
	}

	// Vérifier que l'alerte capturée correspond
	if len(capturedAlerts) > 0 {
		if capturedAlerts[0].CameraID != detection.CameraID {
			t.Errorf("Expected alert camera ID %s, got %s", detection.CameraID, capturedAlerts[0].CameraID)
		}
	}
}

func TestEventProcessor_Stats(t *testing.T) {
	ep := NewEventProcessorEmpty()

	// Stats initiales
	stats := ep.GetStats()
	if stats.TotalDetections != 0 {
		t.Errorf("Expected 0 initial detections, got %d", stats.TotalDetections)
	}

	// Traiter quelques détections
	detection := Detection{
		ID:         uuid.New().String(),
		CameraID:   "test_cam",
		Type:       DetectionTypeMotion,
		Confidence: 0.8,
		Timestamp:  time.Now(),
	}

	ep.ProcessDetection(detection)
	ep.ProcessDetection(detection)

	// Vérifier les stats mises à jour
	stats = ep.GetStats()
	if stats.TotalDetections != 2 {
		t.Errorf("Expected 2 total detections, got %d", stats.TotalDetections)
	}

	// Vérifier que LastProcessed est récent
	if time.Since(stats.LastProcessed) > time.Second {
		t.Errorf("LastProcessed should be recent, got %v", stats.LastProcessed)
	}
}

// Benchmarks pour vérifier les performances
func BenchmarkEventProcessor_ProcessDetection(b *testing.B) {
	ep := NewEventProcessorEmpty()
	ep.RegisterDetector("bench", &mockDetector{shouldAlert: true, alertLevel: AlertLevelInfo})

	detection := Detection{
		ID:         uuid.New().String(),
		CameraID:   "bench_cam",
		Type:       DetectionTypeMotion,
		Confidence: 0.8,
		Timestamp:  time.Now(),
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		ep.ProcessDetection(detection)
	}
}

// Test de concurrence
func TestEventProcessor_Concurrency(t *testing.T) {
	ep := NewEventProcessorEmpty()
	ep.RegisterDetector("concurrent", &mockDetector{shouldAlert: true, alertLevel: AlertLevelInfo})

	// Lancer plusieurs goroutines qui traitent des détections
	done := make(chan bool, 10)

	for i := 0; i < 10; i++ {
		go func(id int) {
			detection := Detection{
				ID:         uuid.New().String(),
				CameraID:   "concurrent_cam",
				Type:       DetectionTypeMotion,
				Confidence: 0.8,
				Timestamp:  time.Now(),
			}

			ep.ProcessDetection(detection)
			done <- true
		}(i)
	}

	// Attendre que toutes les goroutines terminent
	for i := 0; i < 10; i++ {
		<-done
	}

	// Vérifier que toutes les détections ont été traitées
	stats := ep.GetStats()
	if stats.TotalDetections != 10 {
		t.Errorf("Expected 10 total detections, got %d", stats.TotalDetections)
	}
}

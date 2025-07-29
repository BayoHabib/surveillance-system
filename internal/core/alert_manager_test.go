// internal/core/alert_manager_test.go
package core

import (
	"testing"
	"time"

	"github.com/google/uuid"
)

func createTestAlert(cameraID string, level AlertLevel, ageMinutes int) Alert {
	return Alert{
		ID:        uuid.New().String(),
		CameraID:  cameraID,
		Type:      AlertTypeMotion,
		Level:     level,
		Message:   "Test alert",
		Timestamp: time.Now().Add(-time.Duration(ageMinutes) * time.Minute),
	}
}

func TestAlertManager_AddAlert(t *testing.T) {
	am := NewAlertManager(time.Hour)

	alert := createTestAlert("cam_001", AlertLevelWarning, 0)
	am.AddAlert(alert)

	alerts := am.GetAlerts(10, 0)
	if len(alerts) != 1 {
		t.Errorf("Expected 1 alert, got %d", len(alerts))
	}

	if alerts[0].ID != alert.ID {
		t.Errorf("Expected alert ID %s, got %s", alert.ID, alerts[0].ID)
	}
}

func TestAlertManager_GetAlerts_Pagination(t *testing.T) {
	am := NewAlertManager(time.Hour)

	// Ajouter 15 alertes
	for i := 0; i < 15; i++ {
		alert := createTestAlert("cam_001", AlertLevelInfo, i)
		am.AddAlert(alert)
	}

	// Test pagination
	page1 := am.GetAlerts(5, 0)  // Première page : 5 alertes
	page2 := am.GetAlerts(5, 5)  // Deuxième page : 5 alertes
	page3 := am.GetAlerts(5, 10) // Troisième page : 5 alertes

	if len(page1) != 5 {
		t.Errorf("Expected 5 alerts in page1, got %d", len(page1))
	}
	if len(page2) != 5 {
		t.Errorf("Expected 5 alerts in page2, got %d", len(page2))
	}
	if len(page3) != 5 {
		t.Errorf("Expected 5 alerts in page3, got %d", len(page3))
	}

	// Test offset trop grand
	emptyPage := am.GetAlerts(5, 20)
	if len(emptyPage) != 0 {
		t.Errorf("Expected 0 alerts for large offset, got %d", len(emptyPage))
	}
}

func TestAlertManager_GetAlerts_SortedByTime(t *testing.T) {
	am := NewAlertManager(time.Hour)

	// Ajouter des alertes avec différents timestamps
	alert1 := createTestAlert("cam_001", AlertLevelInfo, 10) // Plus ancienne
	alert2 := createTestAlert("cam_002", AlertLevelWarning, 5)
	alert3 := createTestAlert("cam_003", AlertLevelCritical, 1) // Plus récente

	am.AddAlert(alert1)
	am.AddAlert(alert2)
	am.AddAlert(alert3)

	alerts := am.GetAlerts(10, 0)

	// Vérifier l'ordre (plus récente en premier)
	if len(alerts) != 3 {
		t.Fatalf("Expected 3 alerts, got %d", len(alerts))
	}

	if !alerts[0].Timestamp.After(alerts[1].Timestamp) {
		t.Errorf("Alerts should be sorted by timestamp (newest first)")
	}
	if !alerts[1].Timestamp.After(alerts[2].Timestamp) {
		t.Errorf("Alerts should be sorted by timestamp (newest first)")
	}
}

func TestAlertManager_GetAlertsByCamera(t *testing.T) {
	am := NewAlertManager(time.Hour)

	// Ajouter des alertes pour différentes caméras
	alert1 := createTestAlert("cam_001", AlertLevelInfo, 0)
	alert2 := createTestAlert("cam_002", AlertLevelWarning, 0)
	alert3 := createTestAlert("cam_001", AlertLevelCritical, 0)

	am.AddAlert(alert1)
	am.AddAlert(alert2)
	am.AddAlert(alert3)

	// Récupérer les alertes pour cam_001
	cam001Alerts := am.GetAlertsByCamera("cam_001")
	if len(cam001Alerts) != 2 {
		t.Errorf("Expected 2 alerts for cam_001, got %d", len(cam001Alerts))
	}

	// Récupérer les alertes pour cam_002
	cam002Alerts := am.GetAlertsByCamera("cam_002")
	if len(cam002Alerts) != 1 {
		t.Errorf("Expected 1 alert for cam_002, got %d", len(cam002Alerts))
	}

	// Caméra inexistante
	noAlerts := am.GetAlertsByCamera("cam_nonexistent")
	if len(noAlerts) != 0 {
		t.Errorf("Expected 0 alerts for nonexistent camera, got %d", len(noAlerts))
	}
}

func TestAlertManager_AcknowledgeAlert(t *testing.T) {
	am := NewAlertManager(time.Hour)

	alert := createTestAlert("cam_001", AlertLevelWarning, 0)
	am.AddAlert(alert)

	// Acquitter l'alerte
	err := am.AcknowledgeAlert(alert.ID, "user123")
	if err != nil {
		t.Errorf("Unexpected error acknowledging alert: %v", err)
	}

	// Vérifier que l'alerte est acquittée
	alerts := am.GetAlerts(10, 0)
	if len(alerts) != 1 {
		t.Fatalf("Expected 1 alert, got %d", len(alerts))
	}

	ackedAlert := alerts[0]
	if !ackedAlert.Acknowledged {
		t.Errorf("Alert should be acknowledged")
	}
	if ackedAlert.AckedBy != "user123" {
		t.Errorf("Expected AckedBy to be 'user123', got '%s'", ackedAlert.AckedBy)
	}
	if ackedAlert.AckedAt == nil {
		t.Errorf("AckedAt should not be nil")
	}

	// Test acquittement d'une alerte inexistante
	err = am.AcknowledgeAlert("nonexistent", "user123")
	if err == nil {
		t.Errorf("Expected error for nonexistent alert")
	}
}

func TestAlertManager_GetAlertStats(t *testing.T) {
	am := NewAlertManager(time.Hour)

	// Ajouter différents types d'alertes
	alert1 := createTestAlert("cam_001", AlertLevelInfo, 0)
	alert2 := createTestAlert("cam_002", AlertLevelWarning, 0)
	alert3 := createTestAlert("cam_003", AlertLevelCritical, 0)
	alert4 := createTestAlert("cam_004", AlertLevelInfo, 0)

	am.AddAlert(alert1)
	am.AddAlert(alert2)
	am.AddAlert(alert3)
	am.AddAlert(alert4)

	// Acquitter une alerte
	am.AcknowledgeAlert(alert1.ID, "user123")

	// Récupérer les stats
	stats := am.GetAlertStats()

	// Vérifications
	if stats.Total != 4 {
		t.Errorf("Expected 4 total alerts, got %d", stats.Total)
	}
	if stats.Acknowledged != 1 {
		t.Errorf("Expected 1 acknowledged alert, got %d", stats.Acknowledged)
	}
	if stats.Pending != 3 {
		t.Errorf("Expected 3 pending alerts, got %d", stats.Pending)
	}

	// Vérifier les stats par niveau
	if stats.ByLevel[AlertLevelInfo] != 2 {
		t.Errorf("Expected 2 info alerts, got %d", stats.ByLevel[AlertLevelInfo])
	}
	if stats.ByLevel[AlertLevelWarning] != 1 {
		t.Errorf("Expected 1 warning alert, got %d", stats.ByLevel[AlertLevelWarning])
	}
	if stats.ByLevel[AlertLevelCritical] != 1 {
		t.Errorf("Expected 1 critical alert, got %d", stats.ByLevel[AlertLevelCritical])
	}

	// Vérifier les stats par type
	if stats.ByType[AlertTypeMotion] != 4 {
		t.Errorf("Expected 4 motion alerts, got %d", stats.ByType[AlertTypeMotion])
	}
}

func TestAlertManager_CleanupOldAlerts(t *testing.T) {
	// Rétention courte pour le test
	am := NewAlertManager(time.Minute)

	// Ajouter des alertes anciennes et récentes
	oldAlert := createTestAlert("cam_001", AlertLevelInfo, 5)       // 5 minutes (devrait être supprimée)
	recentAlert := createTestAlert("cam_002", AlertLevelWarning, 0) // Récente (devrait rester)

	am.AddAlert(oldAlert)
	am.AddAlert(recentAlert)

	// Nettoyer
	am.CleanupOldAlerts()

	// Vérifier qu'il ne reste que l'alerte récente
	alerts := am.GetAlerts(10, 0)
	if len(alerts) != 1 {
		t.Errorf("Expected 1 alert after cleanup, got %d", len(alerts))
	}

	if len(alerts) > 0 && alerts[0].ID != recentAlert.ID {
		t.Errorf("Wrong alert remained after cleanup")
	}
}

func TestAlertManager_Concurrency(t *testing.T) {
	am := NewAlertManager(time.Hour)

	// Lancer plusieurs goroutines qui ajoutent des alertes
	done := make(chan bool, 10)

	for i := 0; i < 10; i++ {
		go func(id int) {
			alert := createTestAlert("cam_concurrent", AlertLevelInfo, 0)
			am.AddAlert(alert)
			done <- true
		}(i)
	}

	// Attendre que toutes les goroutines terminent
	for i := 0; i < 10; i++ {
		<-done
	}

	// Vérifier que toutes les alertes ont été ajoutées
	alerts := am.GetAlerts(20, 0)
	if len(alerts) != 10 {
		t.Errorf("Expected 10 alerts, got %d", len(alerts))
	}
}

// Benchmark pour les performances
func BenchmarkAlertManager_AddAlert(b *testing.B) {
	am := NewAlertManager(time.Hour)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		alert := createTestAlert("bench_cam", AlertLevelInfo, 0)
		am.AddAlert(alert)
	}
}

func BenchmarkAlertManager_GetAlerts(b *testing.B) {
	am := NewAlertManager(time.Hour)

	// Préparer des données
	for i := 0; i < 1000; i++ {
		alert := createTestAlert("bench_cam", AlertLevelInfo, 0)
		am.AddAlert(alert)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		am.GetAlerts(50, 0)
	}
}

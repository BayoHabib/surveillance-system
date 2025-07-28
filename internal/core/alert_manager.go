// internal/core/alert_manager.go
package core

import (
	"fmt"
	"log"
	"sync"
	"time"
)

type AlertManager interface {
	AddAlert(alert Alert)
	GetAlerts(limit int, offset int) []Alert
	GetAlertsByCamera(cameraID string) []Alert
	AcknowledgeAlert(alertID, userID string) error
	GetAlertStats() AlertStats
	CleanupOldAlerts()
}

type AlertStats struct {
	Total        int `json:"total"`
	Acknowledged int `json:"acknowledged"`
	Pending      int `json:"pending"`
	ByLevel      map[AlertLevel]int `json:"by_level"`
	ByType       map[AlertType]int  `json:"by_type"`
}

type alertManager struct {
	alerts    []Alert
	retention time.Duration
	mutex     sync.RWMutex
}

func NewAlertManager(retention time.Duration) AlertManager {
	am := &alertManager{
		alerts:    make([]Alert, 0),
		retention: retention,
	}
	
	// Nettoyage périodique des anciennes alertes
	go am.periodicCleanup()
	
	return am
}

func (am *alertManager) AddAlert(alert Alert) {
	am.mutex.Lock()
	defer am.mutex.Unlock()
	
	// Insertion triée par timestamp (plus récent en premier)
	insertIndex := 0
	for i, existingAlert := range am.alerts {
		if alert.Timestamp.After(existingAlert.Timestamp) {
			insertIndex = i
			break
		}
		insertIndex = i + 1
	}
	
	// Insertion à l'index calculé
	am.alerts = append(am.alerts, Alert{})
	copy(am.alerts[insertIndex+1:], am.alerts[insertIndex:])
	am.alerts[insertIndex] = alert
}

func (am *alertManager) GetAlerts(limit int, offset int) []Alert {
	am.mutex.RLock()
	defer am.mutex.RUnlock()
	
	if offset >= len(am.alerts) {
		return []Alert{}
	}
	
	end := offset + limit
	if end > len(am.alerts) {
		end = len(am.alerts)
	}
	
	// Copie pour éviter les races conditions
	result := make([]Alert, end-offset)
	copy(result, am.alerts[offset:end])
	
	return result
}

func (am *alertManager) GetAlertsByCamera(cameraID string) []Alert {
	am.mutex.RLock()
	defer am.mutex.RUnlock()
	
	var result []Alert
	for _, alert := range am.alerts {
		if alert.CameraID == cameraID {
			result = append(result, alert)
		}
	}
	
	return result
}

func (am *alertManager) AcknowledgeAlert(alertID, userID string) error {
	am.mutex.Lock()
	defer am.mutex.Unlock()
	
	for i := range am.alerts {
		if am.alerts[i].ID == alertID {
			now := time.Now()
			am.alerts[i].Acknowledged = true
			am.alerts[i].AckedBy = userID
			am.alerts[i].AckedAt = &now
			return nil
		}
	}
	
	return ErrAlertNotFound
}

func (am *alertManager) GetAlertStats() AlertStats {
	am.mutex.RLock()
	defer am.mutex.RUnlock()
	
	stats := AlertStats{
		Total:   len(am.alerts),
		ByLevel: make(map[AlertLevel]int),
		ByType:  make(map[AlertType]int),
	}
	
	for _, alert := range am.alerts {
		if alert.Acknowledged {
			stats.Acknowledged++
		} else {
			stats.Pending++
		}
		
		stats.ByLevel[alert.Level]++
		stats.ByType[alert.Type]++
	}
	
	return stats
}

func (am *alertManager) CleanupOldAlerts() {
	am.mutex.Lock()
	defer am.mutex.Unlock()
	
	cutoff := time.Now().Add(-am.retention)
	
	// Filtrer les alertes récentes
	filtered := make([]Alert, 0)
	for _, alert := range am.alerts {
		if alert.Timestamp.After(cutoff) {
			filtered = append(filtered, alert)
		}
	}
	
	removed := len(am.alerts) - len(filtered)
	am.alerts = filtered
	
	if removed > 0 {
		log.Printf("Nettoyage: %d alertes supprimées", removed)
	}
}

func (am *alertManager) periodicCleanup() {
	ticker := time.NewTicker(time.Hour)
	defer ticker.Stop()
	
	for range ticker.C {
		am.CleanupOldAlerts()
	}
}

// Erreurs
var (
	ErrAlertNotFound = fmt.Errorf("alerte non trouvée")
)

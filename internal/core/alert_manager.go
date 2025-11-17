// internal/core/alert_manager.go
package core

import (
	"errors"
	"log"
	"sort"
	"sync"
	"time"
)

// AlertCallback est appelé quand une nouvelle alerte est créée
type AlertCallback func(alert Alert)

type AlertManager interface {
	AddAlert(alert Alert)
	GetAlerts(limit int, offset int) []Alert
	GetAlertsByCamera(cameraID string) []Alert
	AcknowledgeAlert(alertID, userID string) error
	GetAlertStats() AlertStats
	CleanupOldAlerts()
	SetAlertCallback(callback AlertCallback)
}

type AlertStats struct {
	Total        int                `json:"total"`
	Acknowledged int                `json:"acknowledged"`
	Pending      int                `json:"pending"`
	ByLevel      map[AlertLevel]int `json:"by_level"`
	ByType       map[AlertType]int  `json:"by_type"`
}

type alertManager struct {
	alerts         []Alert
	alertsByCamera map[string][]int // Index: cameraID -> indices dans alerts
	alertQueue     chan Alert        // Canal bufferisé pour ajout asynchrone
	retention      time.Duration
	mutex          sync.RWMutex
	onAlertCreated AlertCallback     // Callback pour notifications temps réel
	callbackMutex  sync.RWMutex      // Mutex pour accès thread-safe au callback
}

func NewAlertManager(retention time.Duration) AlertManager {
	am := &alertManager{
		alerts:         make([]Alert, 0, 5000), // Pré-allouer pour 5000 alertes
		alertsByCamera: make(map[string][]int),
		alertQueue:     make(chan Alert, 10000), // Buffer de 10000 alertes
		retention:      retention,
	}

	// Goroutine pour traitement des alertes en batch
	go am.processAlertQueue()

	// Nettoyage périodique des anciennes alertes
	go am.periodicCleanup()

	return am
}

func (am *alertManager) AddAlert(alert Alert) {
	// Sanitizer et valider
	SanitizeAlert(&alert)
	if validation := ValidateAlert(&alert); validation.HasErrors() {
		log.Printf("❌ Invalid alert rejected: %v (ID: %s, Type: %s)", validation.Errors, alert.ID, alert.Type)
		return
	}
	
	// Envoi non-bloquant dans le canal
	select {
	case am.alertQueue <- alert:
		// Ajouté au canal avec succès
	default:
		// Canal plein - forcer l'ajout synchrone
		log.Printf("⚠️  Alert queue full, adding synchronously")
		am.addAlertDirect(alert)
	}
}

// Ajout direct dans le slice (utilisé en cas de queue pleine)
func (am *alertManager) addAlertDirect(alert Alert) {
	am.mutex.Lock()
	defer am.mutex.Unlock()

	index := len(am.alerts)
	am.alerts = append(am.alerts, alert)
	
	if alert.CameraID != "" {
		am.alertsByCamera[alert.CameraID] = append(am.alertsByCamera[alert.CameraID], index)
	}
}

// Traitement des alertes en batch depuis le canal
func (am *alertManager) processAlertQueue() {
	ticker := time.NewTicker(500 * time.Millisecond) // Batch toutes les 500ms
	defer ticker.Stop()
	
	batch := make([]Alert, 0, 200)
	
	for {
		select {
		case alert := <-am.alertQueue:
			batch = append(batch, alert)
			
			// Si le batch est plein, l'écrire immédiatement
			if len(batch) >= 200 {
				am.writeBatch(batch)
				batch = batch[:0]
			}
			
		case <-ticker.C:
			// Écrire le batch périodiquement même s'il n'est pas plein
			if len(batch) > 0 {
				am.writeBatch(batch)
				batch = batch[:0]
			}
		}
	}
}

// Écriture d'un batch d'alertes
func (am *alertManager) writeBatch(batch []Alert) {
	if len(batch) == 0 {
		return
	}
	
	am.mutex.Lock()
	defer am.mutex.Unlock()
	
	startIndex := len(am.alerts)
	am.alerts = append(am.alerts, batch...)
	
	// Mettre à jour l'index par caméra
	for i, alert := range batch {
		if alert.CameraID != "" {
			index := startIndex + i
			am.alertsByCamera[alert.CameraID] = append(am.alertsByCamera[alert.CameraID], index)
		}
	}
	
	// Log tous les 100 alertes
	if len(am.alerts)%100 < len(batch) {
		log.Printf("✅ Alerts: %d total (%d in this batch)", len(am.alerts), len(batch))
	}
	
	// Notifier via callback pour chaque alerte du batch
	am.callbackMutex.RLock()
	callback := am.onAlertCreated
	am.callbackMutex.RUnlock()
	
	if callback != nil {
		// Appeler le callback pour chaque alerte (en dehors du mutex principal)
		for _, alert := range batch {
			go callback(alert) // Async pour éviter de bloquer
		}
		log.Printf("📤 Notified %d alerts via callback", len(batch))
	} else {
		log.Printf("⚠️  No callback set, %d alerts not broadcasted", len(batch))
	}
}

func (am *alertManager) GetAlerts(limit int, offset int) []Alert {
	am.mutex.RLock()
	defer am.mutex.RUnlock()

	totalAlerts := len(am.alerts)
	if totalAlerts == 0 || offset >= totalAlerts {
		return []Alert{}
	}

	// Optimisation: copier seulement la fenêtre dont on a besoin pour le tri
	// Au lieu de trier TOUTES les alertes, trier seulement les N dernières
	windowSize := offset + limit
	if windowSize > totalAlerts {
		windowSize = totalAlerts
	}
	// Limiter la fenêtre à un maximum raisonnable
	if windowSize > 1000 {
		windowSize = 1000
	}

	// Copier seulement la fenêtre récente
	startIdx := totalAlerts - windowSize
	if startIdx < 0 {
		startIdx = 0
	}
	
	window := make([]Alert, totalAlerts-startIdx)
	copy(window, am.alerts[startIdx:])
	
	// Tri rapide avec sort.Slice - O(n log n) sur la fenêtre seulement
	sort.Slice(window, func(i, j int) bool {
		return window[i].Timestamp.After(window[j].Timestamp)
	})

	// Appliquer offset et limit sur la fenêtre triée
	if offset >= len(window) {
		return []Alert{}
	}

	end := offset + limit
	if end > len(window) {
		end = len(window)
	}

	result := make([]Alert, end-offset)
	copy(result, window[offset:end])

	return result
}

func (am *alertManager) GetAlertsByCamera(cameraID string) []Alert {
	am.mutex.RLock()
	defer am.mutex.RUnlock()

	indices, exists := am.alertsByCamera[cameraID]
	if !exists || len(indices) == 0 {
		return []Alert{}
	}

	result := make([]Alert, 0, len(indices))
	for _, idx := range indices {
		if idx < len(am.alerts) {
			result = append(result, am.alerts[idx])
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

	// Filtrer les alertes récentes et reconstruire l'index
	filtered := make([]Alert, 0)
	newIndexByCamera := make(map[string][]int)
	
	for _, alert := range am.alerts {
		if alert.Timestamp.After(cutoff) {
			newIdx := len(filtered)
			filtered = append(filtered, alert)
			if alert.CameraID != "" {
				newIndexByCamera[alert.CameraID] = append(newIndexByCamera[alert.CameraID], newIdx)
			}
		}
	}

	removed := len(am.alerts) - len(filtered)
	am.alerts = filtered
	am.alertsByCamera = newIndexByCamera

	if removed > 0 {
		log.Printf("🧹 Cleanup: %d alerts removed, %d retained", removed, len(filtered))
	}
}

func (am *alertManager) periodicCleanup() {
	ticker := time.NewTicker(time.Hour)
	defer ticker.Stop()

	for range ticker.C {
		am.CleanupOldAlerts()
	}
}

// SetAlertCallback définit le callback appelé pour chaque nouvelle alerte
func (am *alertManager) SetAlertCallback(callback AlertCallback) {
	am.callbackMutex.Lock()
	defer am.callbackMutex.Unlock()
	am.onAlertCreated = callback
}

// Erreurs
var (
	ErrAlertNotFound = errors.New("alerte non trouvée")
)

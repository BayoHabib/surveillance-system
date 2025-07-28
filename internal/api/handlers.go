// internal/api/handlers.go
package api

import (
	"net/http"
	"strconv"
	"surveillance-core/internal/core"
	"surveillance-core/internal/vision"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type Handler struct {
	visionClient   vision.Client
	eventProcessor core.EventProcessor
	alertManager   core.AlertManager
	cameras        map[string]*core.Camera
	mutex          sync.RWMutex
}

func NewHandler(visionClient vision.Client, eventProcessor core.EventProcessor, alertManager core.AlertManager) *Handler {
	handler := &Handler{
		visionClient:   visionClient,
		eventProcessor: eventProcessor,
		alertManager:   alertManager,
		cameras:        make(map[string]*core.Camera),
	}

	// Ajouter quelques caméras par défaut pour le test
	handler.addDefaultCameras()

	return handler
}

func (h *Handler) addDefaultCameras() {
	cameras := []*core.Camera{
		{
			ID:       "cam_001",
			Name:     "Entrée Principale",
			URL:      "rtsp://mock/entrance",
			Status:   core.CameraStatusOffline,
			Location: "Entrée bâtiment A",
			Config: core.CameraConfig{
				Resolution:   core.Resolution{Width: 1920, Height: 1080},
				FPS:          15,
				Quality:      85,
				EnableMotion: true,
				EnableAI:     false,
			},
			CreatedAt: time.Now(),
			Metadata:  map[string]string{"zone": "public", "priority": "high"},
		},
		{
			ID:       "cam_002",
			Name:     "Couloir Principal",
			URL:      "rtsp://mock/corridor",
			Status:   core.CameraStatusOffline,
			Location: "Couloir niveau 1",
			Config: core.CameraConfig{
				Resolution:   core.Resolution{Width: 1280, Height: 720},
				FPS:          30,
				Quality:      75,
				EnableMotion: true,
				EnableAI:     true,
			},
			CreatedAt: time.Now(),
			Metadata:  map[string]string{"zone": "restricted", "priority": "medium"},
		},
	}

	h.mutex.Lock()
	for _, camera := range cameras {
		h.cameras[camera.ID] = camera
	}
	h.mutex.Unlock()
}

// Endpoints cameras

func (h *Handler) GetCameras(c *gin.Context) {
	h.mutex.RLock()
	cameras := make([]*core.Camera, 0, len(h.cameras))
	for _, camera := range h.cameras {
		cameras = append(cameras, camera)
	}
	h.mutex.RUnlock()

	c.JSON(http.StatusOK, gin.H{
		"cameras": cameras,
		"total":   len(cameras),
	})
}

func (h *Handler) GetCamera(c *gin.Context) {
	cameraID := c.Param("id")

	h.mutex.RLock()
	camera, exists := h.cameras[cameraID]
	h.mutex.RUnlock()

	if !exists {
		c.JSON(http.StatusNotFound, gin.H{"error": "Caméra non trouvée"})
		return
	}

	// Ajouter le statut du stream
	streamStatus := h.visionClient.GetStreamStatus(cameraID)
	response := gin.H{
		"camera":        camera,
		"stream_status": streamStatus,
	}

	c.JSON(http.StatusOK, response)
}

func (h *Handler) CreateCamera(c *gin.Context) {
	var req struct {
		Name     string `json:"name" binding:"required"`
		URL      string `json:"url" binding:"required"`
		Location string `json:"location"`
		Config   core.CameraConfig `json:"config"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	camera := &core.Camera{
		ID:        uuid.New().String(),
		Name:      req.Name,
		URL:       req.URL,
		Status:    core.CameraStatusOffline,
		Location:  req.Location,
		Config:    req.Config,
		CreatedAt: time.Now(),
		Metadata:  make(map[string]string),
	}

	h.mutex.Lock()
	h.cameras[camera.ID] = camera
	h.mutex.Unlock()

	c.JSON(http.StatusCreated, camera)
}

func (h *Handler) StartCamera(c *gin.Context) {
	cameraID := c.Param("id")

	h.mutex.Lock()
	camera, exists := h.cameras[cameraID]
	if !exists {
		h.mutex.Unlock()
		c.JSON(http.StatusNotFound, gin.H{"error": "Caméra non trouvée"})
		return
	}

	// Vérifier si déjà en cours
	if camera.Status == core.CameraStatusStreaming {
		h.mutex.Unlock()
		c.JSON(http.StatusConflict, gin.H{"error": "Caméra déjà en cours de streaming"})
		return
	}

	camera.Status = core.CameraStatusOnline
	h.mutex.Unlock()

	// Démarrer le stream
	framesChan, err := h.visionClient.StartStream(cameraID)
	if err != nil {
		h.mutex.Lock()
		camera.Status = core.CameraStatusError
		h.mutex.Unlock()
		
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Impossible de démarrer le stream"})
		return
	}

	// Traiter les frames en arrière-plan
	go h.processFrames(cameraID, framesChan)

	h.mutex.Lock()
	camera.Status = core.CameraStatusStreaming
	now := time.Now()
	camera.LastFrame = &now
	h.mutex.Unlock()

	c.JSON(http.StatusOK, gin.H{
		"message": "Stream démarré",
		"camera":  camera,
	})
}

func (h *Handler) StopCamera(c *gin.Context) {
	cameraID := c.Param("id")

	h.mutex.Lock()
	camera, exists := h.cameras[cameraID]
	if !exists {
		h.mutex.Unlock()
		c.JSON(http.StatusNotFound, gin.H{"error": "Caméra non trouvée"})
		return
	}
	h.mutex.Unlock()

	// Arrêter le stream
	err := h.visionClient.StopStream(cameraID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erreur arrêt stream"})
		return
	}

	h.mutex.Lock()
	camera.Status = core.CameraStatusOffline
	h.mutex.Unlock()

	c.JSON(http.StatusOK, gin.H{
		"message": "Stream arrêté",
		"camera":  camera,
	})
}

// Endpoint alertes

func (h *Handler) GetAlerts(c *gin.Context) {
	// Paramètres de pagination
	limitStr := c.DefaultQuery("limit", "50")
	offsetStr := c.DefaultQuery("offset", "0")
	cameraID := c.Query("camera_id")

	limit, _ := strconv.Atoi(limitStr)
	offset, _ := strconv.Atoi(offsetStr)

	var alerts []core.Alert
	if cameraID != "" {
		alerts = h.alertManager.GetAlertsByCamera(cameraID)
	} else {
		alerts = h.alertManager.GetAlerts(limit, offset)
	}

	stats := h.alertManager.GetAlertStats()

	c.JSON(http.StatusOK, gin.H{
		"alerts": alerts,
		"stats":  stats,
		"pagination": gin.H{
			"limit":  limit,
			"offset": offset,
			"total":  stats.Total,
		},
	})
}

// Endpoint santé

func (h *Handler) Health(c *gin.Context) {
	visionConnected := h.visionClient.IsConnected()
	processorStats := h.eventProcessor.GetStats()

	status := "healthy"
	if !visionConnected {
		status = "degraded"
	}

	c.JSON(http.StatusOK, gin.H{
		"status":           status,
		"timestamp":        time.Now(),
		"vision_connected": visionConnected,
		"processor_stats":  processorStats,
		"cameras_count":    len(h.cameras),
	})
}

// Traitement des frames en arrière-plan

func (h *Handler) processFrames(cameraID string, framesChan <-chan core.Frame) {
	for frame := range framesChan {
		// Mettre à jour timestamp dernière frame
		h.mutex.Lock()
		if camera, exists := h.cameras[cameraID]; exists {
			camera.LastFrame = &frame.Timestamp
		}
		h.mutex.Unlock()

		// Ici on pourrait appeler l'EventProcessor avec des détections
		// Pour le moment, on simule juste la réception des frames
		
		// Log toutes les 5 secondes pour éviter le spam
		if frame.Timestamp.Second()%5 == 0 {
			println("📹 Frame reçue de", cameraID, "taille:", frame.Size, "bytes")
		}
	}

	// Stream fermé
	h.mutex.Lock()
	if camera, exists := h.cameras[cameraID]; exists {
		camera.Status = core.CameraStatusOffline
	}
	h.mutex.Unlock()

	println("🛑 Stream fermé pour", cameraID)
}

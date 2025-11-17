// internal/api/handlers.go
package api

import (
	"fmt"
	"net/http"
	"net/url"
	"strconv"
	"strings"
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
		Name     string            `json:"name" binding:"required"`
		URL      string            `json:"url" binding:"required"`
		Location string            `json:"location"`
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
	cameraURL := camera.URL // Get the camera URL
	h.mutex.Unlock()

	// Démarrer le stream avec URL
	framesChan, err := h.visionClient.StartStreamWithURL(cameraID, cameraURL)
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

// StreamVideo streams video frames as MJPEG for web display
func (h *Handler) StreamVideo(c *gin.Context) {
	cameraID := c.Param("id")

	h.mutex.RLock()
	camera, exists := h.cameras[cameraID]
	if !exists {
		h.mutex.RUnlock()
		c.JSON(http.StatusNotFound, gin.H{"error": "Caméra non trouvée"})
		return
	}

	if camera.Status != core.CameraStatusStreaming {
		h.mutex.RUnlock()
		c.JSON(http.StatusConflict, gin.H{"error": "Caméra non active"})
		return
	}
	h.mutex.RUnlock()

	// Set headers for MJPEG streaming
	c.Header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
	c.Header("Cache-Control", "no-cache, no-store, must-revalidate")
	c.Header("Pragma", "no-cache")
	c.Header("Expires", "0")
	c.Header("Access-Control-Allow-Origin", "*")

	// Get frame channel from vision client
	framesChan, err := h.visionClient.StartStreamWithURL(cameraID, camera.URL)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get frames"})
		return
	}

	// Stream frames as MJPEG
	flusher, ok := c.Writer.(http.Flusher)
	if !ok {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Streaming not supported"})
		return
	}

	for frame := range framesChan {
		// Convert frame data to JPEG (simplified - in real implementation would use OpenCV)
		if len(frame.Data) == 0 {
			continue
		}

		// Write MJPEG frame boundary
		c.Writer.WriteString("\r\n--frame\r\n")
		c.Writer.WriteString("Content-Type: image/jpeg\r\n")
		c.Writer.WriteString("Content-Length: " + strconv.Itoa(len(frame.Data)) + "\r\n\r\n")
		
		// Write frame data (this would be JPEG in real implementation)
		// For now, we'll create a placeholder
		placeholder := h.createPlaceholderJPEG(cameraID, frame.Width, frame.Height)
		c.Writer.Write(placeholder)
		
		flusher.Flush()

		// Check if client disconnected
		select {
		case <-c.Request.Context().Done():
			return
		default:
		}

		// Limit frame rate for web display
		time.Sleep(time.Millisecond * 66) // ~15 FPS
	}
}

// createPlaceholderJPEG creates a simple placeholder JPEG for testing
func (h *Handler) createPlaceholderJPEG(cameraID string, width, height int) []byte {
	// This is a minimal JPEG header for a placeholder image
	// In real implementation, this would convert OpenCV frames to JPEG
	timestamp := time.Now().Format("15:04:05")
	placeholder := []byte{
		0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
		0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43,
	}
	
	// Add timestamp and camera info to placeholder
	info := []byte("Camera: " + cameraID + " " + timestamp)
	placeholder = append(placeholder, info...)
	
	// Add JPEG end marker
	placeholder = append(placeholder, 0xFF, 0xD9)
	
	return placeholder
}

// Internet Video Streaming Methods

// AddInternetCamera adds a new internet camera stream
func (h *Handler) AddInternetCamera(c *gin.Context) {
	var req struct {
		Name        string `json:"name" binding:"required"`
		URL         string `json:"url" binding:"required"`
		Location    string `json:"location"`
		Type        string `json:"type"` // rtsp, http, rtmp
		Username    string `json:"username"`
		Password    string `json:"password"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Validate URL format
	if err := h.validateStreamURL(req.URL); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid stream URL: " + err.Error()})
		return
	}

	// Generate camera ID
	cameraID := h.generateCameraID(req.Name)

	// Build authenticated URL if credentials provided
	streamURL := req.URL
	if req.Username != "" {
		streamURL = h.buildAuthenticatedURL(req.URL, req.Username, req.Password)
	}

	// Create camera configuration
	camera := &core.Camera{
		ID:        cameraID,
		Name:      req.Name,
		URL:       streamURL,
		Status:    core.CameraStatusOffline,
		Location:  req.Location,
		Config: core.CameraConfig{
			Resolution:   core.Resolution{Width: 1920, Height: 1080},
			FPS:          30,
			Quality:      85,
			EnableMotion: true,
			EnableAI:     false,
		},
		CreatedAt: time.Now(),
		Metadata: map[string]string{
			"type":         req.Type,
			"source":       "internet",
			"original_url": req.URL,
		},
	}

	h.mutex.Lock()
	h.cameras[cameraID] = camera
	h.mutex.Unlock()

	c.JSON(http.StatusCreated, gin.H{
		"camera":  camera,
		"message": "Internet camera added successfully",
	})
}

// GetStreamFormats returns supported streaming formats and examples
func (h *Handler) GetStreamFormats(c *gin.Context) {
	formats := map[string]interface{}{
		"supported_protocols": []string{"rtsp", "http", "https", "rtmp"},
		"examples": map[string]string{
			"rtsp_ip_camera":     "rtsp://username:password@192.168.1.100:554/stream1",
			"http_mjpeg":         "http://camera.example.com:8080/video.mjpeg",
			"youtube_live":       "rtmp://a.rtmp.youtube.com/live2/YOUR_STREAM_KEY",
			"public_webcam":      "http://webcam.example.com/mjpg/video.mjpg",
			"local_file":         "file:///path/to/video.mp4",
		},
		"public_demos": []map[string]string{
			{
				"name":        "Traffic Camera Demo",
				"url":         "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov",
				"type":        "rtsp",
				"description": "Public RTSP test stream",
			},
			{
				"name":        "MJPEG Demo Stream",
				"url":         "http://webcam.buffalotrace.com/mjpg/video.mjpg",
				"type":        "mjpeg",
				"description": "Public MJPEG camera feed",
			},
		},
	}

	c.JSON(http.StatusOK, formats)
}

func (h *Handler) validateStreamURL(streamURL string) error {
	parsedURL, err := url.Parse(streamURL)
	if err != nil {
		return err
	}

	supportedSchemes := []string{"rtsp", "rtmp", "http", "https", "file"}
	scheme := strings.ToLower(parsedURL.Scheme)
	
	for _, supported := range supportedSchemes {
		if scheme == supported {
			return nil
		}
	}

	return fmt.Errorf("unsupported protocol '%s'. Supported: %v", scheme, supportedSchemes)
}

func (h *Handler) buildAuthenticatedURL(streamURL, username, password string) string {
	parsedURL, err := url.Parse(streamURL)
	if err != nil {
		return streamURL
	}

	if username != "" {
		if password != "" {
			parsedURL.User = url.UserPassword(username, password)
		} else {
			parsedURL.User = url.User(username)
		}
	}

	return parsedURL.String()
}

func (h *Handler) generateCameraID(name string) string {
	normalized := strings.ToLower(strings.ReplaceAll(name, " ", "_"))
	return "inet_" + normalized + "_" + fmt.Sprintf("%d", time.Now().Unix()%10000)
}

package main

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"image"
	"image/jpeg"
	"log"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"strconv"
	"surveillance-core/internal/core"
	"surveillance-core/internal/vision"
	pb "surveillance-core/internal/vision/proto"
	wsHub "surveillance-core/internal/websocket"
	"sync"
	"syscall"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

type App struct {
	VisionClient        vision.Client
	EventProcessor      core.EventProcessor
	WSHub               *wsHub.Hub
	AlertManager        core.AlertManager
	DetectionStreamMgr  *vision.DetectionStreamManager
	Config              *core.Config
	ActiveStreams       sync.Map // map[string]*StreamInfo
	Cameras             sync.Map // map[string]*CameraInfo - NEW: Camera storage
	Logger              *log.Logger
}

// CameraInfo stores camera configuration
type CameraInfo struct {
	ID        string    `json:"id"`
	Name      string    `json:"name"`
	URL       string    `json:"url"`
	Location  string    `json:"location"`
	Type      string    `json:"type"`
	Status    string    `json:"status"`
	CreatedAt time.Time `json:"created_at"`
}

type StreamInfo struct {
	ID        string    `json:"id"`
	Name      string    `json:"name"`
	URL       string    `json:"url"`
	Status    string    `json:"status"`
	StartTime time.Time `json:"start_time"`
	FramesCh  <-chan core.Frame
	mu        sync.RWMutex
}

// CameraRequest represents the request structure for adding cameras
type CameraRequest struct {
	Name     string `json:"name" binding:"required"`
	URL      string `json:"url" binding:"required"`
	Location string `json:"location"`
	Type     string `json:"type"`
	Username string `json:"username"`
	Password string `json:"password"`
}

func main() {
	// Initialize structured logger
	logger := log.New(os.Stdout, "[SURVEILLANCE] ", log.LstdFlags|log.Lshortfile)
	
	// Set Gin mode based on environment
	if os.Getenv("GIN_MODE") == "" {
		if os.Getenv("ENVIRONMENT") == "production" {
			gin.SetMode(gin.ReleaseMode)
		} else {
			gin.SetMode(gin.DebugMode)
		}
	}

	// Load configuration with validation
	config, err := core.LoadConfig()
	if err != nil {
		logger.Fatalf("❌ Configuration error: %v", err)
	}

	// Validate critical configuration
	if err := validateConfig(config); err != nil {
		logger.Fatalf("❌ Configuration validation failed: %v", err)
	}

	logger.Printf("🚀 Starting Surveillance System v%s", getVersion())
	logger.Printf("📝 Environment: %s", getEnvironment())
	logger.Printf("🔧 Configuration loaded successfully")

	// Initialize application components
	app, err := initializeApp(config, logger)
	if err != nil {
		logger.Fatalf("❌ Application initialization failed: %v", err)
	}

	// Start WebSocket hub
	go func() {
		logger.Printf("🔌 Starting WebSocket hub...")
		app.WSHub.Run()
	}()

	// Setup HTTP router with middlewares
	router := setupRouter(app)

	// Configure HTTP server with production settings
	server := &http.Server{
		Addr:           config.Server.Port,
		Handler:        router,
		ReadTimeout:    30 * time.Second,  // Default 30 seconds
		WriteTimeout:   30 * time.Second,  // Default 30 seconds
		IdleTimeout:    120 * time.Second, // Default 2 minutes
		MaxHeaderBytes: 1 << 20, // 1 MB
	}

	// Start server in background
	go func() {
		logger.Printf("🌐 Server starting on %s", config.Server.Port)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logger.Fatalf("❌ Server error: %v", err)
		}
	}()

	logger.Printf("✅ Surveillance System started successfully on %s", config.Server.Port)

	// Wait for interrupt signal to gracefully shutdown
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	logger.Println("🛑 Shutdown signal received, starting graceful shutdown...")

	// Create shutdown context with timeout
	ctx, cancel := context.WithTimeout(context.Background(), config.Server.ShutdownTimeout)
	defer cancel()

	// Shutdown server gracefully
	if err := server.Shutdown(ctx); err != nil {
		logger.Printf("❌ Server forced to shutdown: %v", err)
	} else {
		logger.Println("✅ Server shutdown completed successfully")
	}

	// Cleanup application resources
	cleanup(app, logger)
}

func initializeApp(config *core.Config, logger *log.Logger) (*App, error) {
	logger.Printf("🔧 Initializing application components...")

	// Initialize vision client with proper error handling
	visionClient := vision.NewClient(vision.DefaultClientConfig())
	
	// Get gRPC client for detection streams
	var grpcClient pb.VisionServiceClient
	if provider, ok := visionClient.(vision.GRPCClientProvider); ok {
		grpcClient = provider.GetGRPCClient()
		if grpcClient != nil {
			logger.Printf("✅ Using gRPC client for detection streams")
		} else {
			logger.Printf("⚠️  gRPC client is nil, detection streams disabled")
		}
	} else {
		logger.Printf("⚠️  Vision service not connected, detection streams disabled (type: %T)", visionClient)
	}

	// Initialize other components
	eventProcessor := core.NewEventProcessor()
	alertManager := core.NewAlertManager(config.Alerts.Retention)
	hub := wsHub.NewHub()

	// Initialize detection stream manager if gRPC client available
	var detectionStreamMgr *vision.DetectionStreamManager
	if grpcClient != nil {
		detectionStreamMgr = vision.NewDetectionStreamManager(grpcClient, alertManager)
		logger.Printf("✅ Detection stream manager initialized")
	}
	
	// Set up event processing pipeline
	eventProcessor.SetAlertCallback(func(alert core.Alert) {
		logger.Printf("🚨 Alert generated: %s", alert.Message)
		hub.Broadcast(wsHub.Message{
			Type: "alert",
			Data: alert,
		})
	})
	
	// Set up AlertManager callback for real-time WebSocket broadcasting
	alertManager.SetAlertCallback(func(alert core.Alert) {
		hub.Broadcast(wsHub.Message{
			Type: "alert",
			Data: map[string]interface{}{
				"id":          alert.ID,
				"camera_id":   alert.CameraID,
				"type":        alert.Type,
				"level":       alert.Level,
				"message":     alert.Message,
				"timestamp":   alert.Timestamp,
				"motion_pixels": func() interface{} {
					if alert.Detection != nil && alert.Detection.Metadata != nil {
						return alert.Detection.Metadata["motion_pixels"]
					}
					return nil
				}(),
			},
		})
	})

	app := &App{
		VisionClient:        visionClient,
		EventProcessor:      eventProcessor,
		WSHub:               hub,
		AlertManager:        alertManager,
		DetectionStreamMgr:  detectionStreamMgr,
		Config:              config,
		Logger:              logger,
	}

	logger.Printf("✅ Application components initialized successfully")
	return app, nil
}

// validateConfig validates critical configuration parameters
func validateConfig(config *core.Config) error {
	if config.Server.Port == "" {
		return fmt.Errorf("server port cannot be empty")
	}
	
	if config.Server.ShutdownTimeout <= 0 {
		return fmt.Errorf("shutdown timeout must be positive")
	}
	
	if config.Alerts.Retention <= 0 {
		return fmt.Errorf("alert retention must be positive")
	}
	
	return nil
}

// getVersion returns the application version
func getVersion() string {
	version := os.Getenv("APP_VERSION")
	if version == "" {
		return "2.5.0-dev"
	}
	return version
}

// getEnvironment returns the current environment
func getEnvironment() string {
	env := os.Getenv("ENVIRONMENT")
	if env == "" {
		return "development"
	}
	return env
}

// cleanup performs application cleanup on shutdown
func cleanup(app *App, logger *log.Logger) {
	logger.Printf("🧹 Starting cleanup process...")
	
	// Stop all detection streams first
	if app.DetectionStreamMgr != nil {
		logger.Printf("🛑 Stopping all detection streams...")
		app.DetectionStreamMgr.StopAll()
	}
	
	// Stop all active video streams
	app.ActiveStreams.Range(func(key, value interface{}) bool {
		if streamInfo, ok := value.(*StreamInfo); ok {
			logger.Printf("🛑 Stopping stream: %s", streamInfo.ID)
			app.VisionClient.StopStream(streamInfo.ID)
		}
		return true
	})
	
	logger.Printf("✅ Cleanup completed")
}

func setupRouter(app *App) *gin.Engine {
	router := gin.New()
	
	// Production middleware stack
	router.Use(gin.Logger())
	router.Use(gin.Recovery())
	router.Use(securityHeaders())
	router.Use(corsMiddleware())
	router.Use(rateLimitMiddleware())
	router.Use(requestIDMiddleware())

	app.Logger.Printf("🔧 Setting up production router with security middleware...")

	// API v1 routes
	v1 := router.Group("/api/v1")
	{
		// Health and status endpoints
		v1.GET("/health", healthHandler(app))
		v1.GET("/status", statusHandler(app))
		v1.GET("/metrics", metricsHandler(app))

		// Camera management endpoints
		cameras := v1.Group("/cameras")
		{
			cameras.GET("", getCamerasHandler(app))
			cameras.POST("", createCameraHandler(app))
			cameras.GET("/:id", getCameraHandler(app))
			cameras.PUT("/:id", updateCameraHandler(app))
			cameras.DELETE("/:id", deleteCameraHandler(app))
			cameras.PUT("/:id/start", startCameraHandler(app))
			cameras.PUT("/:id/stop", stopCameraHandler(app))
			cameras.GET("/:id/stream", streamHandler(app))
			cameras.GET("/:id/ws-stream", wsVideoStreamHandler(app))  // BUG #5: WebSocket video
		}

		// Internet streaming endpoints
		internet := v1.Group("/cameras/internet")
		{
			internet.POST("", addInternetCameraHandler(app))
			internet.GET("/formats", streamFormatsHandler(app))
		}

		// Alert management
		alerts := v1.Group("/alerts")
		{
			alerts.GET("", getAlertsHandler(app))
			alerts.POST("/:id/acknowledge", acknowledgeAlertHandler(app))
		}
	}

	// WebSocket endpoint
	router.GET("/ws", websocketHandler(app))

	// Static file serving with security headers
	router.Static("/static", "./web/static")
	router.StaticFile("/", "./web/index_enhanced_v2.html")
	router.StaticFile("/old-enhanced", "./web/index_enhanced.html")
	router.StaticFile("/analytics", "./web/analytics.html")
	router.StaticFile("/notifications", "./web/notifications_demo.html")
	router.StaticFile("/snapshot-demo", "./web/snapshot_demo.html")
	router.StaticFile("/poi", "./web/index_poi.html")
	router.StaticFile("/futuristic", "./web/index_futuristic.html")
	router.StaticFile("/modern", "./web/index_modern.html")
	router.StaticFile("/internet", "./web/internet_streaming.html")
	router.StaticFile("/test-notifications", "./web/test_notifications.html")
	router.StaticFile("/explorer", "./web/alert_explorer.html")
	router.StaticFile("/classic", "./web/index_video.html")
	router.StaticFile("/simple", "./web/index_simple.html")

	// 404 handler
	router.NoRoute(func(c *gin.Context) {
		respondNotFound(c, fmt.Sprintf("Endpoint not found: %s %s", c.Request.Method, c.Request.URL.Path))
	})

	app.Logger.Printf("✅ Router setup completed with %d routes", len(router.Routes()))
	return router
}

// Middleware functions
func securityHeaders() gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		c.Header("X-Frame-Options", "DENY")
		c.Header("X-Content-Type-Options", "nosniff")
		c.Header("X-XSS-Protection", "1; mode=block")
		c.Header("Referrer-Policy", "strict-origin-when-cross-origin")
		c.Header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline';")
		c.Next()
	})
}

func corsMiddleware() gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		origin := c.Request.Header.Get("Origin")
		
		// In production, validate origins against allowlist
		if getEnvironment() == "production" {
			allowedOrigins := []string{
				"http://localhost:3000",
				"https://yourdomain.com",
			}
			
			allowed := false
			for _, allowedOrigin := range allowedOrigins {
				if origin == allowedOrigin {
					allowed = true
					break
				}
			}
			
			if allowed {
				c.Header("Access-Control-Allow-Origin", origin)
			}
		} else {
			// Development mode - allow all origins
			c.Header("Access-Control-Allow-Origin", "*")
		}
		
		c.Header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Request-ID")
		c.Header("Access-Control-Allow-Credentials", "true")
		c.Header("Access-Control-Max-Age", "86400")

		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}
		c.Next()
	})
}

func rateLimitMiddleware() gin.HandlerFunc {
	// Simple in-memory rate limiter (in production, use Redis)
	clients := make(map[string][]time.Time)
	var mu sync.Mutex
	
	return gin.HandlerFunc(func(c *gin.Context) {
		clientIP := c.ClientIP()
		now := time.Now()
		
		mu.Lock()
		defer mu.Unlock()
		
		// Clean old requests (older than 1 minute)
		if requests, exists := clients[clientIP]; exists {
			var validRequests []time.Time
			for _, reqTime := range requests {
				if now.Sub(reqTime) < time.Minute {
					validRequests = append(validRequests, reqTime)
				}
			}
			clients[clientIP] = validRequests
		}
		
		// Check rate limit (100 requests per minute)
		if len(clients[clientIP]) >= 100 {
			c.JSON(429, gin.H{
				"error": "rate limit exceeded",
				"retry_after": 60,
			})
			c.Abort()
			return
		}
		
		// Add current request
		clients[clientIP] = append(clients[clientIP], now)
		c.Next()
	})
}

func requestIDMiddleware() gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		requestID := c.Request.Header.Get("X-Request-ID")
		if requestID == "" {
			requestID = generateRequestID()
		}
		c.Header("X-Request-ID", requestID)
		c.Set("RequestID", requestID)
		c.Next()
	})
}

func generateRequestID() string {
	bytes := make([]byte, 16)
	rand.Read(bytes)
	return hex.EncodeToString(bytes)
}

// Error response helpers for consistent API responses
type ErrorResponse struct {
	Error     string                 `json:"error"`
	Message   string                 `json:"message,omitempty"`
	Details   interface{}            `json:"details,omitempty"`
	Code      string                 `json:"code,omitempty"`
	Timestamp time.Time              `json:"timestamp"`
	RequestID string                 `json:"request_id,omitempty"`
	Path      string                 `json:"path,omitempty"`
}

func respondWithError(c *gin.Context, status int, code, message string, details interface{}) {
	requestID, _ := c.Get("RequestID")
	
	response := ErrorResponse{
		Error:     message,
		Code:      code,
		Details:   details,
		Timestamp: time.Now(),
		RequestID: fmt.Sprintf("%v", requestID),
		Path:      c.Request.URL.Path,
	}
	
	c.JSON(status, response)
}

func respondBadRequest(c *gin.Context, message string, details interface{}) {
	respondWithError(c, http.StatusBadRequest, "BAD_REQUEST", message, details)
}

func respondNotFound(c *gin.Context, message string) {
	respondWithError(c, http.StatusNotFound, "NOT_FOUND", message, nil)
}

func respondInternalError(c *gin.Context, message string, err error) {
	details := map[string]interface{}{}
	if err != nil {
		details["error"] = err.Error()
	}
	respondWithError(c, http.StatusInternalServerError, "INTERNAL_ERROR", message, details)
}

func respondConflict(c *gin.Context, message string) {
	respondWithError(c, http.StatusConflict, "CONFLICT", message, nil)
}

func respondTooManyRequests(c *gin.Context, message string) {
	respondWithError(c, http.StatusTooManyRequests, "RATE_LIMIT_EXCEEDED", message, nil)
}

// Handler functions
func healthHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		visionConnected := app.VisionClient.IsConnected()
		
		status := "healthy"
		if !visionConnected {
			status = "degraded"
		}
		
		c.JSON(200, gin.H{
			"status":            status,
			"timestamp":         time.Now(),
			"version":           getVersion(),
			"environment":       getEnvironment(),
			"vision_connected":  visionConnected,
			"uptime":           time.Since(time.Now()).String(), // This would be calculated from start time
			"internet_streaming": "enabled",
		})
	})
}

func statusHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		activeStreams := 0
		app.ActiveStreams.Range(func(key, value interface{}) bool {
			activeStreams++
			return true
		})
		
		c.JSON(200, gin.H{
			"server": gin.H{
				"version":     getVersion(),
				"environment": getEnvironment(),
				"uptime":      time.Since(time.Now()).String(),
			},
			"vision": gin.H{
				"connected": app.VisionClient.IsConnected(),
				"type":      "grpc",
			},
			"streams": gin.H{
				"active": activeStreams,
			},
			"websocket": gin.H{
				"connections": 0, // Would need to implement counter in WSHub
			},
		})
	})
}

func metricsHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		// Basic metrics - in production, use Prometheus
		c.JSON(200, gin.H{
			"metrics": gin.H{
				"requests_total":   0, // Would need to implement counter
				"active_streams":   0, // Count from ActiveStreams
				"alerts_total":     0, // From AlertManager
				"uptime_seconds":   0, // Calculate from start time
			},
		})
	})
}

func getCamerasHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameras := []gin.H{}
		
		// Ajouter les caméras statiques en dur (pour rétrocompatibilité)
		staticCameras := []gin.H{
			{"id": "camera_1", "name": "Front Camera", "status": "offline", "location": "Entrance"},
			{"id": "camera_2", "name": "Back Camera", "status": "offline", "location": "Garden"},
		}
		cameras = append(cameras, staticCameras...)
		
		// Ajouter les caméras actives depuis ActiveStreams
		app.ActiveStreams.Range(func(key, value interface{}) bool {
			if streamInfo, ok := value.(*StreamInfo); ok {
				status := "offline"
				if streamInfo.Status == "streaming" {
					status = "streaming"
				}
				
				cameras = append(cameras, gin.H{
					"id":         streamInfo.ID,
					"name":       streamInfo.Name,
					"status":     status,
					"url":        streamInfo.URL,
					"start_time": streamInfo.StartTime,
				})
			}
			return true
		})
		
		c.JSON(200, gin.H{
			"cameras": cameras,
			"total":   len(cameras),
		})
	})
}

func createCameraHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		var req CameraRequest
		if err := c.ShouldBindJSON(&req); err != nil {
			respondBadRequest(c, "Invalid request format", err.Error())
			return
		}
		
		// Validate URL
		if !isValidURL(req.URL) {
			app.Logger.Printf("❌ Invalid URL rejected: '%s'", req.URL)
			respondBadRequest(c, "Invalid camera URL format", map[string]string{"url": req.URL})
			return
		}
		app.Logger.Printf("✅ URL accepted: '%s'", req.URL)
		
		cameraID := fmt.Sprintf("cam_%d", time.Now().Unix())
		
		// Store camera info
		cameraInfo := &CameraInfo{
			ID:        cameraID,
			Name:      req.Name,
			URL:       req.URL,
			Location:  req.Location,
			Type:      req.Type,
			Status:    "offline",
			CreatedAt: time.Now(),
		}
		app.Cameras.Store(cameraID, cameraInfo)
		
		// In production, save to database
		app.Logger.Printf("📹 Creating camera: %s (%s)", req.Name, cameraID)
		
		c.JSON(201, gin.H{
			"message":    "Camera created successfully",
			"camera_id":  cameraID,
			"name":       req.Name,
			"url":        req.URL,
			"location":   req.Location,
			"status":     "offline",
			"created_at": time.Now(),
		})
	})
}

func getCameraHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameraID := c.Param("id")
		
		// In production, fetch from database
		c.JSON(200, gin.H{
			"id":       cameraID,
			"name":     "Sample Camera",
			"status":   "offline",
			"location": "Unknown",
		})
	})
}

func updateCameraHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameraID := c.Param("id")
		
		var req CameraRequest
		if err := c.ShouldBindJSON(&req); err != nil {
			respondBadRequest(c, "Invalid request format", err.Error())
			return
		}
		
		app.Logger.Printf("📝 Updating camera: %s", cameraID)
		
		c.JSON(200, gin.H{
			"message":    "Camera updated successfully",
			"camera_id":  cameraID,
			"updated_at": time.Now(),
		})
	})
}

func deleteCameraHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameraID := c.Param("id")
		
		// BUG #1 FIX: Stop stream if active and cleanup resources
		err := app.VisionClient.StopStream(cameraID)
		if err != nil {
			app.Logger.Printf("⚠️  Warning: failed to stop stream for camera %s: %v", cameraID, err)
		}
		
		// Stop detection stream if active
		if app.DetectionStreamMgr != nil {
			app.DetectionStreamMgr.StopDetectionStream(cameraID)
		}
		
		// Delete from ActiveStreams (this cleans up the StreamInfo)
		app.ActiveStreams.Delete(cameraID)
		
		app.Logger.Printf("🗑️  Deleting camera: %s", cameraID)
		
		c.JSON(200, gin.H{
			"message":    "Camera deleted successfully",
			"camera_id":  cameraID,
			"deleted_at": time.Now(),
		})
	})
}

func startCameraHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameraID := c.Param("id")
		
		// Get camera info
		camInfoRaw, exists := app.Cameras.Load(cameraID)
		if !exists {
			respondNotFound(c, "Camera not found")
			return
		}
		camInfo := camInfoRaw.(*CameraInfo)
		
		// Start stream with actual URL
		framesChan, err := app.VisionClient.StartStreamWithURL(cameraID, camInfo.URL)
		if err != nil {
			respondInternalError(c, "Failed to start camera stream", err)
			return
		}
		
		// Update camera status
		camInfo.Status = "active"
		app.Cameras.Store(cameraID, camInfo)
		
		// Store stream info
		streamInfo := &StreamInfo{
			ID:        cameraID,
			Name:      camInfo.Name,
			URL:       camInfo.URL,
			Status:    "streaming",
			StartTime: time.Now(),
			FramesCh:  framesChan,
		}
		app.ActiveStreams.Store(cameraID, streamInfo)
		
		// Start detection stream if manager available
		if app.DetectionStreamMgr != nil {
			if err := app.DetectionStreamMgr.StartDetectionStream(cameraID); err != nil {
				app.Logger.Printf("⚠️  Failed to start detection stream for %s: %v", cameraID, err)
			} else {
				app.Logger.Printf("🔍 Detection stream started for camera: %s", cameraID)
			}
		}
		
		app.Logger.Printf("▶️  Started camera stream: %s", cameraID)
		
		c.JSON(200, gin.H{
			"message":    "Camera stream started",
			"camera_id":  cameraID,
			"status":     "streaming",
			"started_at": time.Now(),
		})
	})
}

func stopCameraHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameraID := c.Param("id")
		
		err := app.VisionClient.StopStream(cameraID)
		if err != nil {
			respondInternalError(c, "Failed to stop camera stream", err)
			return
		}
		
		app.ActiveStreams.Delete(cameraID)
		
		// Stop detection stream if manager available
		if app.DetectionStreamMgr != nil {
			app.DetectionStreamMgr.StopDetectionStream(cameraID)
			app.Logger.Printf("🔍 Detection stream stopped for camera: %s", cameraID)
		}
		
		app.Logger.Printf("⏹️  Stopped camera stream: %s", cameraID)
		
		c.JSON(200, gin.H{
			"message":    "Camera stream stopped",
			"camera_id":  cameraID,
			"status":     "offline",
			"stopped_at": time.Now(),
		})
	})
}

// convertBGRToJPEG converts BGR raw pixel data to JPEG format
func convertBGRToJPEG(bgrData []byte, width, height int) ([]byte, error) {
	// Create RGBA image
	img := image.NewRGBA(image.Rect(0, 0, width, height))
	
	// Convert BGR to RGBA
	// BGR data is packed as: B, G, R, B, G, R, ...
	pixelCount := width * height
	expectedSize := pixelCount * 3
	
	if len(bgrData) < expectedSize {
		return nil, fmt.Errorf("insufficient BGR data: got %d bytes, expected at least %d", len(bgrData), expectedSize)
	}
	
	for i := 0; i < pixelCount; i++ {
		bgrIdx := i * 3
		rgbaIdx := i * 4
		
		// BGR -> RGBA conversion
		img.Pix[rgbaIdx+0] = bgrData[bgrIdx+2] // R
		img.Pix[rgbaIdx+1] = bgrData[bgrIdx+1] // G
		img.Pix[rgbaIdx+2] = bgrData[bgrIdx+0] // B
		img.Pix[rgbaIdx+3] = 255               // A (fully opaque)
	}
	
	// Encode to JPEG
	var buf bytes.Buffer
	opts := &jpeg.Options{Quality: 85} // Good quality/size balance
	if err := jpeg.Encode(&buf, img, opts); err != nil {
		return nil, fmt.Errorf("failed to encode JPEG: %w", err)
	}
	
	return buf.Bytes(), nil
}

func streamHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameraID := c.Param("id")

		// Set MJPEG headers
		c.Header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
		c.Header("Cache-Control", "no-cache, no-store, must-revalidate")
		c.Header("Connection", "keep-alive")
		c.Header("Pragma", "no-cache")
		c.Header("Expires", "0")

		// Try to get frames from ActiveStreams first (internet cameras)
		var frames <-chan core.Frame
		var err error
		
		if streamInfo, ok := app.ActiveStreams.Load(cameraID); ok {
			if info, ok := streamInfo.(*StreamInfo); ok {
				frames = info.FramesCh
				app.Logger.Printf("📺 Starting MJPEG stream for internet camera: %s", cameraID)
			}
		}
		
		// If not in ActiveStreams, try vision client (local cameras)
		if frames == nil {
			frames, err = app.VisionClient.GetStream(cameraID)
			if err != nil {
				app.Logger.Printf("❌ Error getting stream for camera %s: %v", cameraID, err)
				respondNotFound(c, fmt.Sprintf("Stream not found for camera: %s", cameraID))
				return
			}
			app.Logger.Printf("📺 Starting MJPEG stream for local camera: %s", cameraID)
		}

		// BUG #2 FIX: Context pour détecter déconnexion client
		ctx := c.Request.Context()
		done := make(chan struct{})
		
		// Goroutine pour détecter fermeture connexion
		go func() {
			<-ctx.Done()
			close(done)
		}()

		// Stream frames and convert BGR to JPEG
		frameCount := 0
		streamStartTime := time.Now()
		
		// BUG #4 FIX: Frame skipping pour clients lents
		skippedFrames := 0
		skipInterval := 1  // Envoyer 1 frame sur N (1 = pas de skip)
		lastSkipCheck := time.Now()
		
		for {
			select {
			case frame, ok := <-frames:
				if !ok {
					app.Logger.Printf("📺 Frame channel closed for camera: %s", cameraID)
					return
				}
				
				frameCount++
				
				// BUG #4 FIX: Adapter le skip interval selon la buffer utilization
				// Vérifier toutes les 30 frames (environ 1 seconde @ 30 FPS)
				if frameCount%30 == 0 && time.Since(lastSkipCheck) > time.Second {
					// Estimer la charge du buffer en mesurant le nombre de frames en attente
					// Si len(frames) est élevé, le client est lent
					bufferLoad := len(frames)
					bufferCapacity := cap(frames)
					
					if bufferCapacity > 0 {
						loadPercent := (bufferLoad * 100) / bufferCapacity
						
						if loadPercent > 80 {
							// Client très lent: skip 2 frames sur 3
							skipInterval = 3
							app.Logger.Printf("⚠️ Camera %s: High buffer load (%d%%), skipping 2/3 frames", 
								cameraID, loadPercent)
						} else if loadPercent > 60 {
							// Client modérément lent: skip 1 frame sur 2
							skipInterval = 2
							app.Logger.Printf("⚠️ Camera %s: Medium buffer load (%d%%), skipping 1/2 frames", 
								cameraID, loadPercent)
						} else {
							// Client OK: pas de skip
							if skipInterval > 1 {
								app.Logger.Printf("✓ Camera %s: Buffer load normal (%d%%), resuming full rate", 
									cameraID, loadPercent)
							}
							skipInterval = 1
						}
					}
					
					lastSkipCheck = time.Now()
				}
				
				// Skip frames si nécessaire (garder 1 frame sur skipInterval)
				if skipInterval > 1 && frameCount%skipInterval != 0 {
					skippedFrames++
					continue  // Skip cette frame
				}
				
				// Convert BGR frame data to JPEG
				jpegData, err := convertBGRToJPEG(frame.Data, frame.Width, frame.Height)
				if err != nil {
					app.Logger.Printf("⚠️ Failed to convert frame %d for camera %s: %v", frameCount, cameraID, err)
					continue
				}
				
				// Write MJPEG boundary and frame
				if _, err := c.Writer.Write([]byte("--frame\r\n")); err != nil {
					app.Logger.Printf("📺 Client disconnected (boundary write failed): %s", cameraID)
					return
				}
				if _, err := c.Writer.Write([]byte("Content-Type: image/jpeg\r\n")); err != nil {
					app.Logger.Printf("📺 Client disconnected (header write failed): %s", cameraID)
					return
				}
				if _, err := c.Writer.Write([]byte(fmt.Sprintf("Content-Length: %d\r\n\r\n", len(jpegData)))); err != nil {
					app.Logger.Printf("📺 Client disconnected (length write failed): %s", cameraID)
					return
				}
				if _, err := c.Writer.Write(jpegData); err != nil {
					app.Logger.Printf("📺 Client disconnected (data write failed): %s", cameraID)
					return
				}
				if _, err := c.Writer.Write([]byte("\r\n")); err != nil {
					app.Logger.Printf("📺 Client disconnected (end write failed): %s", cameraID)
					return
				}
				
				if flusher, ok := c.Writer.(http.Flusher); ok {
					flusher.Flush()
				}
				
				if frameCount%100 == 0 {
					elapsed := time.Since(streamStartTime).Seconds()
					fps := float64(frameCount) / elapsed
					effectiveFPS := float64(frameCount-skippedFrames) / elapsed
					skipRate := float64(skippedFrames) / float64(frameCount) * 100
					app.Logger.Printf("📸 Camera %s: %d frames (%.0f%% skipped), %.1f FPS received, %.1f FPS sent", 
						cameraID, frameCount, skipRate, fps, effectiveFPS)
				}
				
			case <-done:
				elapsed := time.Since(streamStartTime).Seconds()
				fps := float64(frameCount) / elapsed
				effectiveFPS := float64(frameCount-skippedFrames) / elapsed
				skipRate := float64(skippedFrames) / float64(frameCount) * 100
				app.Logger.Printf("📺 MJPEG stream stopped for camera %s: %d frames in %.1fs (%.1f FPS received, %.1f FPS sent, %.0f%% skipped)", 
					cameraID, frameCount, elapsed, fps, effectiveFPS, skipRate)
				return
				
			case <-time.After(10 * time.Second):
				// Timeout si aucune frame pendant 10 secondes
				app.Logger.Printf("⚠️ Stream timeout for camera %s (no frames for 10s)", cameraID)
				return
			}
		}
	})
}

// BUG #5 FIX: WebSocket video streaming handler
var wsVideoUpgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024 * 64, // 64KB buffer pour frames
	CheckOrigin: func(r *http.Request) bool {
		return true // Allow all origins in development
	},
}

func wsVideoStreamHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		cameraID := c.Param("id")
		
		// Upgrade HTTP connection to WebSocket
		conn, err := wsVideoUpgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			app.Logger.Printf("❌ WebSocket upgrade failed for camera %s: %v", cameraID, err)
			return
		}
		defer conn.Close()
		
		app.Logger.Printf("🔌 WebSocket video stream started for camera: %s from %s", cameraID, c.ClientIP())
		
		// Get frames channel
		var frames <-chan core.Frame
		
		// First, check ActiveStreams
		if streamInfo, ok := app.ActiveStreams.Load(cameraID); ok {
			if info, ok := streamInfo.(*StreamInfo); ok {
				frames = info.FramesCh
				app.Logger.Printf("✅ Using existing ActiveStreams channel for camera: %s", cameraID)
			}
		}
		
		// If not in ActiveStreams, try to get from VisionClient
		if frames == nil {
			frames, err = app.VisionClient.GetStream(cameraID)
			if err != nil {
				// Stream doesn't exist, need to start it
				app.Logger.Printf("⚠️ Stream not found for camera %s, checking if we can start it...", cameraID)
				
				// We can't automatically start streams without camera URL stored somewhere
				// For now, return error message
				app.Logger.Printf("❌ No stream found for camera: %s", cameraID)
				conn.WriteMessage(websocket.TextMessage, []byte(fmt.Sprintf(`{"error":"Stream not found. Please start the camera stream first via POST /api/v1/cameras/internet","camera_id":"%s"}`, cameraID)))
				return
			} else {
				app.Logger.Printf("✅ Using existing VisionClient stream for camera: %s", cameraID)
			}
		}
		
		// Context pour détecter déconnexion
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()
		
		// Goroutine pour lire les messages du client (ping/pong)
		go func() {
			defer cancel()
			for {
				if _, _, err := conn.ReadMessage(); err != nil {
					app.Logger.Printf("🔌 WebSocket client disconnected: %s", cameraID)
					return
				}
			}
		}()
		
		// Stream frames via WebSocket
		frameCount := 0
		streamStartTime := time.Now()
		lastPingTime := time.Now()
		
		// Configure ping/pong
		conn.SetReadDeadline(time.Now().Add(60 * time.Second))
		conn.SetPongHandler(func(string) error {
			conn.SetReadDeadline(time.Now().Add(60 * time.Second))
			return nil
		})
		
		for {
			select {
			case frame, ok := <-frames:
				if !ok {
					app.Logger.Printf("🔌 Frame channel closed for WebSocket: %s", cameraID)
					return
				}
				
				frameCount++
				
				// Convert BGR to JPEG
				jpegData, err := convertBGRToJPEG(frame.Data, frame.Width, frame.Height)
				if err != nil {
					app.Logger.Printf("⚠️ Failed to convert frame for WebSocket: %v", err)
					continue
				}
				
				// Send frame as binary message
				if err := conn.WriteMessage(websocket.BinaryMessage, jpegData); err != nil {
					app.Logger.Printf("🔌 WebSocket write failed for camera %s: %v", cameraID, err)
					return
				}
				
				// Send ping every 10 seconds
				if time.Since(lastPingTime) > 10*time.Second {
					if err := conn.WriteMessage(websocket.PingMessage, nil); err != nil {
						app.Logger.Printf("🔌 WebSocket ping failed: %v", err)
						return
					}
					lastPingTime = time.Now()
				}
				
				if frameCount%100 == 0 {
					elapsed := time.Since(streamStartTime).Seconds()
					fps := float64(frameCount) / elapsed
					app.Logger.Printf("🔌 WebSocket camera %s: %d frames, %.1f FPS", cameraID, frameCount, fps)
				}
				
			case <-ctx.Done():
				elapsed := time.Since(streamStartTime).Seconds()
				fps := float64(frameCount) / elapsed
				app.Logger.Printf("🔌 WebSocket stream stopped for camera %s: %d frames in %.1fs (%.1f FPS)", 
					cameraID, frameCount, elapsed, fps)
				return
				
			case <-time.After(15 * time.Second):
				// Timeout si aucune frame
				app.Logger.Printf("⚠️ WebSocket timeout for camera %s", cameraID)
				return
			}
		}
	})
}

func addInternetCameraHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		var req CameraRequest
		if err := c.ShouldBindJSON(&req); err != nil {
			respondBadRequest(c, "Invalid request format", err.Error())
			return
		}

		// Validate URL
		if !isValidURL(req.URL) {
			respondBadRequest(c, "Invalid camera URL format", map[string]string{"url": req.URL})
			return
		}

		cameraID := fmt.Sprintf("internet_%d", time.Now().Unix())

	// Start internet stream (this also starts the stream on vision service for detection)
	framesChan, err := app.VisionClient.StartStreamWithURL(cameraID, req.URL)
	if err != nil {
		respondInternalError(c, "Failed to start internet stream", err)
		return
	}

	// Wait for camera initialization in C++
	time.Sleep(2 * time.Second)

	// Store stream info
	streamInfo := &StreamInfo{
		ID:        cameraID,
		Name:      req.Name,
		URL:       req.URL,
		Status:    "streaming",
		StartTime: time.Now(),
		FramesCh:  framesChan,
	}
	app.ActiveStreams.Store(cameraID, streamInfo)

	// Start detection stream if manager available
	if app.DetectionStreamMgr != nil {
		if err := app.DetectionStreamMgr.StartDetectionStream(cameraID); err != nil {
			app.Logger.Printf("⚠️  Failed to start detection stream for %s: %v", cameraID, err)
		} else {
			app.Logger.Printf("🔍 Detection stream started for camera: %s", cameraID)
		}
	}

	app.Logger.Printf("🌐 Internet camera added: %s (%s) from URL: %s", req.Name, cameraID, req.URL)

	c.JSON(201, gin.H{
		"message":          "Internet camera added successfully",
		"camera_id":        cameraID,
		"name":             req.Name,
		"url":              req.URL,
		"status":           "streaming",
			"resolution":       "1280x720",
			"fps":              30,
			"frames_available": len(framesChan) > 0,
			"created_at":       time.Now(),
		})
	})
}

func streamFormatsHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		c.JSON(200, gin.H{
			"formats": []gin.H{
				{"protocol": "rtsp", "description": "Real Time Streaming Protocol for IP cameras", "example": "rtsp://username:password@192.168.1.100:554/stream"},
				{"protocol": "http", "description": "HTTP video streams and MJPEG", "example": "http://camera-ip:8080/video"},
				{"protocol": "https", "description": "Secure HTTP video streams", "example": "https://example.com/stream.m3u8"},
				{"protocol": "rtmp", "description": "Real Time Messaging Protocol", "example": "rtmp://live-server.com/live/stream-key"},
			},
			"demo_streams": []gin.H{
				{"name": "Big Buck Bunny RTSP", "url": "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4"},
				{"name": "Sample MP4", "url": "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"},
				{"name": "Tears of Steel", "url": "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/TearsOfSteel.mp4"},
			},
		})
	})
}

func getAlertsHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		app.Logger.Printf("📊 GET /api/v1/alerts - Start")
		
		// Paramètres de pagination
		limitStr := c.DefaultQuery("limit", "50")
		offsetStr := c.DefaultQuery("offset", "0")
		cameraID := c.Query("camera_id")

		limit, _ := strconv.Atoi(limitStr)
		offset, _ := strconv.Atoi(offsetStr)

		app.Logger.Printf("📊 Fetching alerts: limit=%d, offset=%d, camera=%s", limit, offset, cameraID)

		var alerts []core.Alert
		if cameraID != "" {
			alerts = app.AlertManager.GetAlertsByCamera(cameraID)
		} else {
			alerts = app.AlertManager.GetAlerts(limit, offset)
		}

		app.Logger.Printf("📊 Retrieved %d alerts", len(alerts))

		stats := app.AlertManager.GetAlertStats()

		app.Logger.Printf("📊 Stats: total=%d", stats.Total)

		c.JSON(200, gin.H{
			"alerts": alerts,
			"stats":  stats,
			"pagination": gin.H{
				"limit":  limit,
				"offset": offset,
				"total":  stats.Total,
			},
		})
	})
}

func acknowledgeAlertHandler(app *App) gin.HandlerFunc {
	return gin.HandlerFunc(func(c *gin.Context) {
		alertID := c.Param("id")
		
		app.Logger.Printf("✅ Alert acknowledged: %s", alertID)
		
		c.JSON(200, gin.H{
			"message":        "Alert acknowledged",
			"alert_id":       alertID,
			"acknowledged_at": time.Now(),
		})
	})
}

func websocketHandler(app *App) gin.HandlerFunc {
	handler := wsHub.NewHandler(app.WSHub)
	
	return gin.HandlerFunc(func(c *gin.Context) {
		app.Logger.Printf("🔌 WebSocket connection request from %s", c.ClientIP())
		handler.HandleWebSocket(c.Writer, c.Request)
	})
}

// Utility functions
func isValidURL(urlStr string) bool {
	if urlStr == "" {
		return false
	}
	
	parsedURL, err := url.Parse(urlStr)
	if err != nil {
		return false
	}
	
	// Check for valid schemes
	validSchemes := []string{"http", "https", "rtsp", "rtmp", "file"}
	for _, scheme := range validSchemes {
		if parsedURL.Scheme == scheme {
			return true
		}
	}
	
	return false
}

package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/gin-gonic/gin"
	"surveillance-core/internal/api"
	"surveillance-core/internal/core"
	"surveillance-core/internal/vision"
	wsHub "surveillance-core/internal/websocket"
)

type App struct {
	VisionClient   vision.Client
	EventProcessor core.EventProcessor
	WSHub          *wsHub.Hub
	AlertManager   core.AlertManager
}

func main() {
	// Configuration
	config := &core.Config{
		Port:           ":8080",
		VisionService:  "localhost:50051", // gRPC C++ service
		MaxCameras:     10,
		AlertRetention: time.Hour * 24,
	}

	// Initialisation des composants
	app := initializeApp(config)

	// Démarrage du Hub WebSocket
	go app.WSHub.Run()

	// Démarrage du serveur HTTP
	router := setupRouter(app)
	
	server := &http.Server{
		Addr:    config.Port,
		Handler: router,
	}

	// Graceful shutdown
	go func() {
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("Erreur serveur: %v", err)
		}
	}()

	log.Printf("Serveur démarré sur %s", config.Port)

	// Attente signal d'arrêt
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("Arrêt du serveur...")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := server.Shutdown(ctx); err != nil {
		log.Fatalf("Erreur arrêt serveur: %v", err)
	}
}

func initializeApp(config *core.Config) *App {
	// Vision client (avec mock pour Phase 1)
	visionClient := vision.NewMockClient()
	
	// Event processor
	eventProcessor := core.NewEventProcessor()
	
	// Alert manager
	alertManager := core.NewAlertManager(config.AlertRetention)
	
	// WebSocket Hub
	hub := wsHub.NewHub()

	// Connexion event processor -> websocket
	eventProcessor.SetAlertCallback(func(alert core.Alert) {
		hub.Broadcast(wsHub.Message{
			Type: "alert",
			Data: alert,
		})
	})

	return &App{
		VisionClient:   visionClient,
		EventProcessor: eventProcessor,
		WSHub:          hub,
		AlertManager:   alertManager,
	}
}

func setupRouter(app *App) *gin.Engine {
	router := gin.Default()

	// CORS middleware
	router.Use(func(c *gin.Context) {
		c.Header("Access-Control-Allow-Origin", "*")
		c.Header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Content-Type, Authorization")

		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}
		c.Next()
	})

	// API routes
	apiHandler := api.NewHandler(app.VisionClient, app.EventProcessor, app.AlertManager)
	
	v1 := router.Group("/api/v1")
	{
		v1.GET("/cameras", apiHandler.GetCameras)
		v1.POST("/cameras", apiHandler.CreateCamera)
		v1.GET("/cameras/:id", apiHandler.GetCamera)
		v1.PUT("/cameras/:id/start", apiHandler.StartCamera)
		v1.PUT("/cameras/:id/stop", apiHandler.StopCamera)
		v1.GET("/alerts", apiHandler.GetAlerts)
		v1.GET("/health", apiHandler.Health)
	}

	// WebSocket endpoint
	wsHandler := wsHub.NewHandler(app.WSHub)
	router.GET("/ws", func(c *gin.Context) {
		wsHandler.HandleWebSocket(c.Writer, c.Request)
	})

	// Static files pour le frontend
	router.Static("/static", "./web/static")
	router.StaticFile("/", "./web/index.html")

	return router
}

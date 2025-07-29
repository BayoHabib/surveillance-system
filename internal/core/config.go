// internal/core/config.go
package core

import (
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

// Config représente la configuration globale améliorée
type Config struct {
	// Serveur
	Server ServerConfig `json:"server"`

	// Services externes
	VisionService VisionServiceConfig `json:"vision_service"`

	// Base de données
	Database DatabaseConfig `json:"database"`

	// Caméras
	Cameras CameraManagerConfig `json:"cameras"`

	// Alertes
	Alerts AlertConfig `json:"alerts"`

	// Logs
	Logging LoggingConfig `json:"logging"`

	// Sécurité
	Security SecurityConfig `json:"security"`
}

type ServerConfig struct {
	Port            string        `json:"port"`
	Host            string        `json:"host"`
	ReadTimeout     time.Duration `json:"read_timeout"`
	WriteTimeout    time.Duration `json:"write_timeout"`
	ShutdownTimeout time.Duration `json:"shutdown_timeout"`
	TLSEnabled      bool          `json:"tls_enabled"`
	CertFile        string        `json:"cert_file"`
	KeyFile         string        `json:"key_file"`
}

type VisionServiceConfig struct {
	Address             string        `json:"address"`
	Timeout             time.Duration `json:"timeout"`
	MaxRetries          int           `json:"max_retries"`
	RetryInterval       time.Duration `json:"retry_interval"`
	HealthCheckInterval time.Duration `json:"health_check_interval"`
}

type CameraManagerConfig struct {
	MaxCameras           int           `json:"max_cameras"`
	DefaultFPS           int           `json:"default_fps"`
	DefaultQuality       int           `json:"default_quality"`
	StreamTimeout        time.Duration `json:"stream_timeout"`
	ReconnectDelay       time.Duration `json:"reconnect_delay"`
	MaxReconnectAttempts int           `json:"max_reconnect_attempts"`
}

type AlertConfig struct {
	Retention            time.Duration         `json:"retention"`
	MaxAlerts            int                   `json:"max_alerts"`
	NotificationChannels []NotificationChannel `json:"notification_channels"`
	ThrottleInterval     time.Duration         `json:"throttle_interval"`
	BatchSize            int                   `json:"batch_size"`
}

type NotificationChannel struct {
	Name    string            `json:"name"`
	Type    string            `json:"type"` // "email", "webhook", "sms"
	Enabled bool              `json:"enabled"`
	Config  map[string]string `json:"config"`
	Filters []AlertFilter     `json:"filters"`
}

type AlertFilter struct {
	Level    AlertLevel `json:"level,omitempty"`
	Type     AlertType  `json:"type,omitempty"`
	CameraID string     `json:"camera_id,omitempty"`
}

type LoggingConfig struct {
	Level      string `json:"level"`
	Format     string `json:"format"` // "json", "text"
	Output     string `json:"output"` // "stdout", "file", "both"
	File       string `json:"file"`
	MaxSize    int    `json:"max_size"` // MB
	MaxAge     int    `json:"max_age"`  // Days
	MaxBackups int    `json:"max_backups"`
	Compress   bool   `json:"compress"`
}

type SecurityConfig struct {
	JWTSecret        string        `json:"jwt_secret"`
	JWTExpiry        time.Duration `json:"jwt_expiry"`
	RateLimitEnabled bool          `json:"rate_limit_enabled"`
	RateLimitRPS     int           `json:"rate_limit_rps"`
	CORSEnabled      bool          `json:"cors_enabled"`
	CORSOrigins      []string      `json:"cors_origins"`
}

// LoadConfig charge la configuration depuis les variables d'environnement avec des valeurs par défaut
func LoadConfig() (*Config, error) {
	config := getDefaultConfig()

	// Charger depuis les variables d'environnement
	if err := loadFromEnv(config); err != nil {
		return nil, fmt.Errorf("failed to load config from environment: %w", err)
	}

	// Valider la configuration
	if err := validateConfig(config); err != nil {
		return nil, fmt.Errorf("invalid configuration: %w", err)
	}

	return config, nil
}

// getDefaultConfig retourne une configuration par défaut
func getDefaultConfig() *Config {
	return &Config{
		Server: ServerConfig{
			Port:            ":8080",
			Host:            "0.0.0.0",
			ReadTimeout:     15 * time.Second,
			WriteTimeout:    15 * time.Second,
			ShutdownTimeout: 5 * time.Second,
			TLSEnabled:      false,
		},
		VisionService: VisionServiceConfig{
			Address:             "localhost:50051",
			Timeout:             30 * time.Second,
			MaxRetries:          3,
			RetryInterval:       5 * time.Second,
			HealthCheckInterval: 30 * time.Second,
		},
		Database: DatabaseConfig{
			Type:     "sqlite",
			URL:      "surveillance.db",
			MaxConns: 10,
		},
		Cameras: CameraManagerConfig{
			MaxCameras:           10,
			DefaultFPS:           15,
			DefaultQuality:       75,
			StreamTimeout:        30 * time.Second,
			ReconnectDelay:       5 * time.Second,
			MaxReconnectAttempts: 5,
		},
		Alerts: AlertConfig{
			Retention:        24 * time.Hour,
			MaxAlerts:        1000,
			ThrottleInterval: 30 * time.Second,
			BatchSize:        10,
			NotificationChannels: []NotificationChannel{
				{
					Name:    "default",
					Type:    "webhook",
					Enabled: false,
					Config:  map[string]string{},
				},
			},
		},
		Logging: LoggingConfig{
			Level:      "info",
			Format:     "text",
			Output:     "stdout",
			MaxSize:    10,
			MaxAge:     7,
			MaxBackups: 3,
			Compress:   true,
		},
		Security: SecurityConfig{
			JWTSecret:        "change-me-in-production",
			JWTExpiry:        24 * time.Hour,
			RateLimitEnabled: true,
			RateLimitRPS:     100,
			CORSEnabled:      true,
			CORSOrigins:      []string{"*"},
		},
	}
}

// loadFromEnv charge les valeurs depuis les variables d'environnement
func loadFromEnv(config *Config) error {
	// Serveur
	if port := os.Getenv("PORT"); port != "" {
		if !strings.HasPrefix(port, ":") {
			port = ":" + port
		}
		config.Server.Port = port
	}
	if host := os.Getenv("HOST"); host != "" {
		config.Server.Host = host
	}
	if tlsStr := os.Getenv("TLS_ENABLED"); tlsStr != "" {
		if tls, err := strconv.ParseBool(tlsStr); err == nil {
			config.Server.TLSEnabled = tls
		}
	}
	if certFile := os.Getenv("TLS_CERT_FILE"); certFile != "" {
		config.Server.CertFile = certFile
	}
	if keyFile := os.Getenv("TLS_KEY_FILE"); keyFile != "" {
		config.Server.KeyFile = keyFile
	}

	// Service Vision
	if addr := os.Getenv("VISION_SERVICE_ADDRESS"); addr != "" {
		config.VisionService.Address = addr
	}
	if timeoutStr := os.Getenv("VISION_SERVICE_TIMEOUT"); timeoutStr != "" {
		if timeout, err := time.ParseDuration(timeoutStr); err == nil {
			config.VisionService.Timeout = timeout
		}
	}

	// Base de données
	if dbType := os.Getenv("DB_TYPE"); dbType != "" {
		config.Database.Type = dbType
	}
	if dbURL := os.Getenv("DB_URL"); dbURL != "" {
		config.Database.URL = dbURL
	}
	if maxConnsStr := os.Getenv("DB_MAX_CONNS"); maxConnsStr != "" {
		if maxConns, err := strconv.Atoi(maxConnsStr); err == nil {
			config.Database.MaxConns = maxConns
		}
	}

	// Caméras
	if maxCamsStr := os.Getenv("MAX_CAMERAS"); maxCamsStr != "" {
		if maxCams, err := strconv.Atoi(maxCamsStr); err == nil {
			config.Cameras.MaxCameras = maxCams
		}
	}
	if defaultFPSStr := os.Getenv("DEFAULT_FPS"); defaultFPSStr != "" {
		if fps, err := strconv.Atoi(defaultFPSStr); err == nil {
			config.Cameras.DefaultFPS = fps
		}
	}

	// Alertes
	if retentionStr := os.Getenv("ALERT_RETENTION"); retentionStr != "" {
		if retention, err := time.ParseDuration(retentionStr); err == nil {
			config.Alerts.Retention = retention
		}
	}
	if maxAlertsStr := os.Getenv("MAX_ALERTS"); maxAlertsStr != "" {
		if maxAlerts, err := strconv.Atoi(maxAlertsStr); err == nil {
			config.Alerts.MaxAlerts = maxAlerts
		}
	}

	// Logs
	if level := os.Getenv("LOG_LEVEL"); level != "" {
		config.Logging.Level = level
	}
	if format := os.Getenv("LOG_FORMAT"); format != "" {
		config.Logging.Format = format
	}
	if output := os.Getenv("LOG_OUTPUT"); output != "" {
		config.Logging.Output = output
	}
	if file := os.Getenv("LOG_FILE"); file != "" {
		config.Logging.File = file
	}

	// Sécurité
	if secret := os.Getenv("JWT_SECRET"); secret != "" {
		config.Security.JWTSecret = secret
	}
	if expiryStr := os.Getenv("JWT_EXPIRY"); expiryStr != "" {
		if expiry, err := time.ParseDuration(expiryStr); err == nil {
			config.Security.JWTExpiry = expiry
		}
	}
	if rateLimitStr := os.Getenv("RATE_LIMIT_ENABLED"); rateLimitStr != "" {
		if enabled, err := strconv.ParseBool(rateLimitStr); err == nil {
			config.Security.RateLimitEnabled = enabled
		}
	}

	return nil
}

// validateConfig valide la configuration
func validateConfig(config *Config) error {
	// Valider le port
	if config.Server.Port == "" {
		return fmt.Errorf("server port is required")
	}

	// Valider TLS
	if config.Server.TLSEnabled {
		if config.Server.CertFile == "" || config.Server.KeyFile == "" {
			return fmt.Errorf("TLS cert and key files required when TLS is enabled")
		}
	}

	// Valider le service Vision
	if config.VisionService.Address == "" {
		return fmt.Errorf("vision service address is required")
	}

	// Valider les caméras
	if config.Cameras.MaxCameras <= 0 {
		return fmt.Errorf("max cameras must be positive")
	}
	if config.Cameras.DefaultFPS <= 0 || config.Cameras.DefaultFPS > 120 {
		return fmt.Errorf("default FPS must be between 1 and 120")
	}

	// Valider les alertes
	if config.Alerts.Retention <= 0 {
		return fmt.Errorf("alert retention must be positive")
	}
	if config.Alerts.MaxAlerts <= 0 {
		return fmt.Errorf("max alerts must be positive")
	}

	// Valider les logs
	validLogLevels := map[string]bool{
		"debug": true, "info": true, "warn": true, "error": true,
	}
	if !validLogLevels[config.Logging.Level] {
		return fmt.Errorf("invalid log level: %s", config.Logging.Level)
	}

	// Valider la sécurité
	if config.Security.JWTSecret == "change-me-in-production" {
		fmt.Println("WARNING: Using default JWT secret in production is insecure!")
	}

	return nil
}

// IsDevelopment retourne true si l'environnement est en développement
func (c *Config) IsDevelopment() bool {
	env := strings.ToLower(os.Getenv("ENV"))
	return env == "dev" || env == "development" || env == ""
}

// IsProduction retourne true si l'environnement est en production
func (c *Config) IsProduction() bool {
	env := strings.ToLower(os.Getenv("ENV"))
	return env == "prod" || env == "production"
}

// GetServerAddress retourne l'adresse complète du serveur
func (c *Config) GetServerAddress() string {
	return c.Server.Host + c.Server.Port
}

// String retourne une représentation string de la config (sans secrets)
func (c *Config) String() string {
	safeConfig := *c
	safeConfig.Security.JWTSecret = "[REDACTED]"

	return fmt.Sprintf("Config{Server: %+v, Cameras: %+v, Alerts: %+v}",
		safeConfig.Server, safeConfig.Cameras, safeConfig.Alerts)
}

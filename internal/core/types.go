// internal/core/types.go
package core

import (
	"time"
)

// Camera représente une caméra de surveillance
type Camera struct {
	ID          string            `json:"id"`
	Name        string            `json:"name"`
	URL         string            `json:"url"`
	Status      CameraStatus      `json:"status"`
	Location    string            `json:"location"`
	Config      CameraConfig      `json:"config"`
	CreatedAt   time.Time         `json:"created_at"`
	LastFrame   *time.Time        `json:"last_frame,omitempty"`
	Metadata    map[string]string `json:"metadata"`
}

type CameraStatus string

const (
	CameraStatusOffline    CameraStatus = "offline"
	CameraStatusOnline     CameraStatus = "online"
	CameraStatusStreaming  CameraStatus = "streaming"
	CameraStatusError      CameraStatus = "error"
)

type CameraConfig struct {
	Resolution   Resolution `json:"resolution"`
	FPS          int        `json:"fps"`
	Quality      int        `json:"quality"` // 1-100
	EnableMotion bool       `json:"enable_motion"`
	EnableAI     bool       `json:"enable_ai"`
	Zones        []Zone     `json:"zones"`
}

type Resolution struct {
	Width  int `json:"width"`
	Height int `json:"height"`
}

type Zone struct {
	ID     string    `json:"id"`
	Name   string    `json:"name"`
	Points []Point   `json:"points"` // Polygone de détection
	Active bool      `json:"active"`
}

type Point struct {
	X int `json:"x"`
	Y int `json:"y"`
}

// Detection représente une détection par l'IA
type Detection struct {
	ID         string            `json:"id"`
	CameraID   string            `json:"camera_id"`
	Type       DetectionType     `json:"type"`
	Confidence float32           `json:"confidence"`
	BBox       BoundingBox       `json:"bbox"`
	Timestamp  time.Time         `json:"timestamp"`
	Metadata   map[string]string `json:"metadata"`
}

type DetectionType string

const (
	DetectionTypeMotion    DetectionType = "motion"
	DetectionTypePerson    DetectionType = "person"
	DetectionTypeFace      DetectionType = "face"
	DetectionTypeVehicle   DetectionType = "vehicle"
	DetectionTypeObject    DetectionType = "object"
	DetectionTypeIntrusion DetectionType = "intrusion"
)

type BoundingBox struct {
	X      int `json:"x"`
	Y      int `json:"y"`
	Width  int `json:"width"`
	Height int `json:"height"`
}

// Alert représente une alerte générée
type Alert struct {
	ID          string            `json:"id"`
	CameraID    string            `json:"camera_id"`
	Type        AlertType         `json:"type"`
	Level       AlertLevel        `json:"level"`
	Message     string            `json:"message"`
	Detection   *Detection        `json:"detection,omitempty"`
	Timestamp   time.Time         `json:"timestamp"`
	Acknowledged bool             `json:"acknowledged"`
	AckedBy     string            `json:"acked_by,omitempty"`
	AckedAt     *time.Time        `json:"acked_at,omitempty"`
	Metadata    map[string]string `json:"metadata"`
}

type AlertType string

const (
	AlertTypeMotion    AlertType = "motion_detected"
	AlertTypeIntrusion AlertType = "intrusion_detected"
	AlertTypeFace      AlertType = "unauthorized_face"
	AlertTypeSystem    AlertType = "system_alert"
)

type AlertLevel string

const (
	AlertLevelInfo     AlertLevel = "info"
	AlertLevelWarning  AlertLevel = "warning"
	AlertLevelCritical AlertLevel = "critical"
	AlertLevelUrgent   AlertLevel = "urgent"
)

// Frame représente une image de caméra
type Frame struct {
	CameraID  string    `json:"camera_id"`
	Data      []byte    `json:"-"` // Image data
	Width     int       `json:"width"`
	Height    int       `json:"height"`
	Format    string    `json:"format"` // "jpeg", "png", etc.
	Timestamp time.Time `json:"timestamp"`
	Size      int       `json:"size"`
}

// Config représente la configuration globale
type Config struct {
	Port           string
	VisionService  string
	MaxCameras     int
	AlertRetention time.Duration
	LogLevel       string
	Database       DatabaseConfig
}

type DatabaseConfig struct {
	Type     string // "sqlite", "postgres", etc.
	URL      string
	MaxConns int
}

// Event représente un événement du système
type Event struct {
	ID        string                 `json:"id"`
	Type      string                 `json:"type"`
	Source    string                 `json:"source"`
	Data      map[string]interface{} `json:"data"`
	Timestamp time.Time              `json:"timestamp"`
}

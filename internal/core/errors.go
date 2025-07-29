// internal/core/errors.go
package core

import (
	"errors"
	"fmt"
	"net/url"
	"strings"
)

// Erreurs système personnalisées
var (
	ErrCameraNotFound       = errors.New("camera not found")
	ErrCameraAlreadyExists  = errors.New("camera already exists")
	ErrStreamNotFound       = errors.New("stream not found")
	ErrStreamAlreadyActive  = errors.New("stream already active")
	ErrInvalidCameraID      = errors.New("invalid camera ID")
	ErrInvalidURL           = errors.New("invalid camera URL")
	ErrInvalidConfiguration = errors.New("invalid configuration")
	//ErrAlertNotFound      = errors.New("alert not found")
	ErrDetectorNotFound   = errors.New("detector not found")
	ErrInvalidDetection   = errors.New("invalid detection data")
	ErrServiceUnavailable = errors.New("service unavailable")
)

// SystemError représente une erreur avec contexte
type SystemError struct {
	Code    string
	Message string
	Cause   error
	Context map[string]interface{}
}

func (e *SystemError) Error() string {
	if e.Cause != nil {
		return fmt.Sprintf("%s: %s (caused by: %v)", e.Code, e.Message, e.Cause)
	}
	return fmt.Sprintf("%s: %s", e.Code, e.Message)
}

func (e *SystemError) Unwrap() error {
	return e.Cause
}

// Constructeurs d'erreurs avec contexte
func NewCameraError(code, message string, cause error, cameraID string) *SystemError {
	return &SystemError{
		Code:    code,
		Message: message,
		Cause:   cause,
		Context: map[string]interface{}{
			"camera_id": cameraID,
		},
	}
}

func NewStreamError(code, message string, cause error, cameraID string, streamStatus StreamStatus) *SystemError {
	return &SystemError{
		Code:    code,
		Message: message,
		Cause:   cause,
		Context: map[string]interface{}{
			"camera_id":     cameraID,
			"stream_status": streamStatus,
		},
	}
}

func NewValidationError(field, message string, value interface{}) *SystemError {
	return &SystemError{
		Code:    "VALIDATION_ERROR",
		Message: fmt.Sprintf("validation failed for field '%s': %s", field, message),
		Context: map[string]interface{}{
			"field": field,
			"value": value,
		},
	}
}

// ValidationResult représente le résultat d'une validation
type ValidationResult struct {
	Valid  bool
	Errors []ValidationError
}

type ValidationError struct {
	Field   string      `json:"field"`
	Message string      `json:"message"`
	Value   interface{} `json:"value,omitempty"`
}

func (vr *ValidationResult) AddError(field, message string, value interface{}) {
	vr.Valid = false
	vr.Errors = append(vr.Errors, ValidationError{
		Field:   field,
		Message: message,
		Value:   value,
	})
}

func (vr *ValidationResult) HasErrors() bool {
	return len(vr.Errors) > 0
}

// Validateurs

// ValidateCamera valide les données d'une caméra
func ValidateCamera(camera *Camera) *ValidationResult {
	result := &ValidationResult{Valid: true}

	// ID requis et format valide
	if camera.ID == "" {
		result.AddError("id", "camera ID is required", camera.ID)
	} else if len(camera.ID) < 3 || len(camera.ID) > 50 {
		result.AddError("id", "camera ID must be between 3 and 50 characters", camera.ID)
	} else if !isValidCameraID(camera.ID) {
		result.AddError("id", "camera ID contains invalid characters", camera.ID)
	}

	// Nom requis
	if camera.Name == "" {
		result.AddError("name", "camera name is required", camera.Name)
	} else if len(camera.Name) > 100 {
		result.AddError("name", "camera name must be less than 100 characters", camera.Name)
	}

	// URL valide
	if camera.URL == "" {
		result.AddError("url", "camera URL is required", camera.URL)
	} else if !isValidCameraURL(camera.URL) {
		result.AddError("url", "camera URL format is invalid", camera.URL)
	}

	// Configuration
	if err := validateCameraConfig(&camera.Config); err != nil {
		result.AddError("config", err.Error(), camera.Config)
	}

	return result
}

// ValidateDetection valide une détection
func ValidateDetection(detection *Detection) *ValidationResult {
	result := &ValidationResult{Valid: true}

	// ID requis
	if detection.ID == "" {
		result.AddError("id", "detection ID is required", detection.ID)
	}

	// Camera ID requis
	if detection.CameraID == "" {
		result.AddError("camera_id", "camera ID is required", detection.CameraID)
	}

	// Type valide
	if !isValidDetectionType(detection.Type) {
		result.AddError("type", "invalid detection type", detection.Type)
	}

	// Confiance entre 0 et 1
	if detection.Confidence < 0 || detection.Confidence > 1 {
		result.AddError("confidence", "confidence must be between 0 and 1", detection.Confidence)
	}

	// Bounding box valide
	if detection.BBox.Width <= 0 || detection.BBox.Height <= 0 {
		result.AddError("bbox", "bounding box dimensions must be positive", detection.BBox)
	}

	// Timestamp valide
	if detection.Timestamp.IsZero() {
		result.AddError("timestamp", "timestamp is required", detection.Timestamp)
	}

	return result
}

// ValidateAlert valide une alerte
func ValidateAlert(alert *Alert) *ValidationResult {
	result := &ValidationResult{Valid: true}

	// ID requis
	if alert.ID == "" {
		result.AddError("id", "alert ID is required", alert.ID)
	}

	// Camera ID requis
	if alert.CameraID == "" {
		result.AddError("camera_id", "camera ID is required", alert.CameraID)
	}

	// Type valide
	if !isValidAlertType(alert.Type) {
		result.AddError("type", "invalid alert type", alert.Type)
	}

	// Niveau valide
	if !isValidAlertLevel(alert.Level) {
		result.AddError("level", "invalid alert level", alert.Level)
	}

	// Message requis
	if alert.Message == "" {
		result.AddError("message", "alert message is required", alert.Message)
	}

	// Timestamp valide
	if alert.Timestamp.IsZero() {
		result.AddError("timestamp", "timestamp is required", alert.Timestamp)
	}

	return result
}

// Fonctions utilitaires de validation

func isValidCameraID(id string) bool {
	// Autorise lettres, chiffres, tirets et underscores
	for _, r := range id {
		if !((r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') ||
			(r >= '0' && r <= '9') || r == '-' || r == '_') {
			return false
		}
	}
	return true
}

func isValidCameraURL(urlStr string) bool {
	// Vérifier le format de base de l'URL
	u, err := url.Parse(urlStr)
	if err != nil {
		return false
	}

	// Accepter différents schémas
	validSchemes := map[string]bool{
		"http":  true,
		"https": true,
		"rtsp":  true,
		"rtmp":  true,
		"mock":  true, // Pour les tests
	}

	return validSchemes[strings.ToLower(u.Scheme)]
}

func validateCameraConfig(config *CameraConfig) error {
	// Résolution valide
	if config.Resolution.Width <= 0 || config.Resolution.Height <= 0 {
		return fmt.Errorf("resolution must have positive dimensions")
	}
	if config.Resolution.Width > 7680 || config.Resolution.Height > 4320 { // 8K max
		return fmt.Errorf("resolution exceeds maximum supported (8K)")
	}

	// FPS valide
	if config.FPS <= 0 || config.FPS > 120 {
		return fmt.Errorf("FPS must be between 1 and 120")
	}

	// Qualité valide
	if config.Quality < 1 || config.Quality > 100 {
		return fmt.Errorf("quality must be between 1 and 100")
	}

	// Valider les zones
	for i, zone := range config.Zones {
		if zone.ID == "" {
			return fmt.Errorf("zone %d must have an ID", i)
		}
		if zone.Name == "" {
			return fmt.Errorf("zone %d must have a name", i)
		}
		if len(zone.Points) < 3 {
			return fmt.Errorf("zone %d must have at least 3 points", i)
		}
	}

	return nil
}

func isValidDetectionType(detType DetectionType) bool {
	validTypes := map[DetectionType]bool{
		DetectionTypeMotion:    true,
		DetectionTypePerson:    true,
		DetectionTypeFace:      true,
		DetectionTypeVehicle:   true,
		DetectionTypeObject:    true,
		DetectionTypeIntrusion: true,
	}
	return validTypes[detType]
}

func isValidAlertType(alertType AlertType) bool {
	validTypes := map[AlertType]bool{
		AlertTypeMotion:    true,
		AlertTypeIntrusion: true,
		AlertTypeFace:      true,
		AlertTypeSystem:    true,
	}
	return validTypes[alertType]
}

func isValidAlertLevel(level AlertLevel) bool {
	validLevels := map[AlertLevel]bool{
		AlertLevelInfo:     true,
		AlertLevelWarning:  true,
		AlertLevelCritical: true,
		AlertLevelUrgent:   true,
	}
	return validLevels[level]
}

// Sanitizers pour nettoyer les données

func SanitizeCamera(camera *Camera) {
	camera.Name = strings.TrimSpace(camera.Name)
	camera.Location = strings.TrimSpace(camera.Location)
	camera.URL = strings.TrimSpace(camera.URL)

	// Nettoyer les métadonnées
	for key, value := range camera.Metadata {
		camera.Metadata[key] = strings.TrimSpace(value)
	}
}

func SanitizeAlert(alert *Alert) {
	alert.Message = strings.TrimSpace(alert.Message)
	alert.AckedBy = strings.TrimSpace(alert.AckedBy)

	// Nettoyer les métadonnées
	for key, value := range alert.Metadata {
		alert.Metadata[key] = strings.TrimSpace(value)
	}
}

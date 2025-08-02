// src/frame_processor.cpp
#include "frame_processor.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <cstring>

using namespace FrameProcessorConstants;

// =============================================================================
// BasicMotionDetector Implementation
// =============================================================================

BasicMotionDetector::BasicMotionDetector() 
    : initialized_(false), detection_counter_(0), 
      motion_threshold_(DEFAULT_MOTION_THRESHOLD), min_area_(DEFAULT_MIN_AREA) {
}

BasicMotionDetector::~BasicMotionDetector() {
    Cleanup();
}

bool BasicMotionDetector::Initialize() {
    initialized_ = true;
    detection_counter_ = 0;
    return true;
}

void BasicMotionDetector::Cleanup() {
    initialized_ = false;
    previous_frame_ = Frame();
}

std::vector<Detection> BasicMotionDetector::Detect(const Frame& frame) {
    std::vector<Detection> detections;
    
    if (!initialized_) {
        return detections;
    }
    
    // Pour Phase 2.1, simulation simple de détection de mouvement
    if (previous_frame_.data.empty()) {
        // Première frame, juste la stocker
        previous_frame_ = frame;
        return detections;
    }
    
    // Vérifier s'il y a un changement significatif
    if (HasSignificantChange(frame, previous_frame_)) {
        // Créer une détection simulée
        int x = 100 + (detection_counter_ % 400);  // Position variable
        int y = 100 + ((detection_counter_ / 10) % 200);
        int width = 80 + (detection_counter_ % 40);
        int height = 60 + (detection_counter_ % 30);
        float confidence = 0.7f + static_cast<float>(rand()) / RAND_MAX * 0.3f;  // 0.7-1.0
        
        Detection detection = CreateMotionDetection(x, y, width, height, confidence);
        detections.push_back(detection);
        
        detection_counter_++;
    }
    
    // Mettre à jour la frame précédente
    previous_frame_ = frame;
    
    return detections;
}

bool BasicMotionDetector::HasSignificantChange(const Frame& current, const Frame& previous) const {
    // Simulation simple basée sur la taille des données et un peu d'aléatoire
    if (current.data.size() != previous.data.size()) {
        return true;
    }
    
    // Simulation : mouvement détecté environ 30% du temps
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);
    
    return dis(gen) < 0.3;  // 30% de chance de détection
}

Detection BasicMotionDetector::CreateMotionDetection(int x, int y, int width, int height, float confidence) const {
    Detection detection;
    detection.set_id(GenerateDetectionId());
    detection.set_type("motion");
    detection.set_confidence(confidence);
    detection.set_timestamp(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    
    // Bounding box
    BoundingBox* bbox = detection.mutable_bbox();
    bbox->set_x(x);
    bbox->set_y(y);
    bbox->set_width(width);
    bbox->set_height(height);
    
    // Métadonnées
    auto& metadata = *detection.mutable_metadata();
    metadata["detector"] = "BasicMotionDetector";
    metadata["algorithm"] = "simulated";
    metadata["confidence_str"] = std::to_string(confidence);
    
    return detection;
}

std::string BasicMotionDetector::GenerateDetectionId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();
    
    return "motion_" + std::to_string(timestamp) + "_" + std::to_string(detection_counter_.load());
}

// =============================================================================
// FrameProcessor Implementation  
// =============================================================================

FrameProcessor::FrameProcessor() 
    : initialized_(false), motion_threshold_(DEFAULT_MOTION_THRESHOLD),
      min_detection_area_(DEFAULT_MIN_AREA), max_detections_per_frame_(DEFAULT_MAX_DETECTIONS) {
}

FrameProcessor::~FrameProcessor() {
    Cleanup();
}

bool FrameProcessor::Initialize() {
    if (initialized_) {
        return true;
    }
    
    // Ajouter le détecteur de mouvement par défaut
    auto motion_detector = std::make_unique<BasicMotionDetector>();
    if (!motion_detector->Initialize()) {
        return false;
    }
    
    detectors_.push_back(std::move(motion_detector));
    
    initialized_ = true;
    return true;
}

void FrameProcessor::Cleanup() {
    for (auto& detector : detectors_) {
        if (detector) {
            detector->Cleanup();
        }
    }
    detectors_.clear();
    initialized_ = false;
}

ProcessingResult FrameProcessor::ProcessFrame(const Frame& frame) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (!initialized_) {
        return CreateErrorResult("FrameProcessor not initialized");
    }
    
    if (!ValidateFrame(frame)) {
        return CreateErrorResult("Invalid frame data");
    }
    
    ProcessingResult result;
    
    try {
        // Appliquer tous les détecteurs
        for (const auto& detector : detectors_) {
            if (detector) {
                std::vector<Detection> detections = detector->Detect(frame);
                
                // Ajouter les détections au résultat
                for (const auto& detection : detections) {
                    if (result.detections.size() < static_cast<size_t>(max_detections_per_frame_)) {
                        result.detections.push_back(detection);
                    } else {
                        break;  // Limite atteinte
                    }
                }
            }
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result = CreateErrorResult("Processing error: " + std::string(e.what()));
    }
    
    // Calculer le temps de traitement
    auto end_time = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();
    
    // Mettre à jour les statistiques
    UpdateStatistics(result.processing_time_ms, static_cast<int>(result.detections.size()));
    
    return result;
}

ProcessingResult FrameProcessor::ProcessFrame(const std::vector<uint8_t>& frame_data,
                                             int width, int height,
                                             const std::string& format) {
    Frame frame(width, height, format);
    frame.data = frame_data;
    frame.timestamp = std::chrono::steady_clock::now();
    
    return ProcessFrame(frame);
}

void FrameProcessor::AddDetector(std::unique_ptr<Detector> detector) {
    if (detector && detector->Initialize()) {
        detectors_.push_back(std::move(detector));
    }
}

void FrameProcessor::RemoveDetector(const std::string& detector_name) {
    detectors_.erase(
        std::remove_if(detectors_.begin(), detectors_.end(),
                      [&detector_name](const std::unique_ptr<Detector>& detector) {
                          return detector && detector->GetName() == detector_name;
                      }),
        detectors_.end()
    );
}

std::vector<std::string> FrameProcessor::GetDetectorNames() const {
    std::vector<std::string> names;
    for (const auto& detector : detectors_) {
        if (detector) {
            names.push_back(detector->GetName());
        }
    }
    return names;
}

void FrameProcessor::SetMotionThreshold(double threshold) {
    motion_threshold_ = std::clamp(threshold, 0.0, 1.0);
}

void FrameProcessor::SetMinDetectionArea(int area) {
    min_detection_area_ = std::max(1, area);
}

void FrameProcessor::SetMaxDetectionsPerFrame(int max_detections) {
    max_detections_per_frame_ = std::max(1, max_detections);
}

int64_t FrameProcessor::GetTotalFramesProcessed() const {
    return total_frames_processed_.load();
}

int64_t FrameProcessor::GetTotalDetections() const {
    return total_detections_.load();
}

double FrameProcessor::GetAverageProcessingTime() const {
    int64_t frames = total_frames_processed_.load();
    if (frames == 0) return 0.0;
    
    return static_cast<double>(total_processing_time_.load()) / frames;
}

bool FrameProcessor::ValidateFrame(const Frame& frame) const {
    if (frame.data.empty()) {
        return false;
    }
    
    if (frame.width < MIN_FRAME_WIDTH || frame.width > MAX_FRAME_WIDTH ||
        frame.height < MIN_FRAME_HEIGHT || frame.height > MAX_FRAME_HEIGHT) {
        return false;
    }
    
    if (frame.format.empty()) {
        return false;
    }
    
    // Vérifier que la taille des données correspond aux dimensions
    size_t expected_size = FrameUtils::CalculateFrameSize(frame.width, frame.height, frame.format);
    if (expected_size > 0 && frame.data.size() < expected_size * 0.8) {  // Tolérance de 20%
        return false;
    }
    
    return true;
}

ProcessingResult FrameProcessor::CreateErrorResult(const std::string& error) const {
    ProcessingResult result;
    result.success = false;
    result.error_message = error;
    result.processing_time_ms = 0;
    return result;
}

void FrameProcessor::UpdateStatistics(int64_t processing_time, int detections_count) {
    total_frames_processed_++;
    total_detections_ += detections_count;
    total_processing_time_ += processing_time;
}

// =============================================================================
// FrameUtils Implementation
// =============================================================================

namespace FrameUtils {

std::vector<uint8_t> ConvertFormat(const std::vector<uint8_t>& data,
                                  int width, int height,
                                  const std::string& from_format,
                                  const std::string& to_format) {
    // Pour Phase 2.1, conversion simple (juste copie)
    // Sera amélioré avec OpenCV en Phase 2.3
    if (from_format == to_format) {
        return data;
    }
    
    // Conversions basiques
    if (from_format == "bgr" && to_format == "rgb") {
        std::vector<uint8_t> converted = data;
        // Échanger B et R
        for (size_t i = 0; i < converted.size(); i += 3) {
            std::swap(converted[i], converted[i + 2]);
        }
        return converted;
    }
    
    // Par défaut, retourner les données originales
    return data;
}

bool IsValidFormat(const std::string& format) {
    return std::find(SUPPORTED_FORMATS.begin(), SUPPORTED_FORMATS.end(), format) 
           != SUPPORTED_FORMATS.end();
}

std::vector<std::string> GetSupportedFormats() {
    return SUPPORTED_FORMATS;
}

size_t CalculateFrameSize(int width, int height, const std::string& format) {
    if (format == "bgr" || format == "rgb") {
        return width * height * 3;
    } else if (format == "gray") {
        return width * height;
    } else if (format == "jpeg" || format == "png") {
        // Estimation pour les formats compressés
        return width * height * 3 / 2;  // Approximation
    }
    
    return 0;  // Format inconnu
}

Frame CreateTestFrame(int width, int height, const std::string& format) {
    Frame frame(width, height, format);
    
    size_t frame_size = CalculateFrameSize(width, height, format);
    frame.data.resize(frame_size);
    
    // Remplir avec un pattern de test
    if (format == "bgr" || format == "rgb") {
        // Pattern coloré
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = (y * width + x) * 3;
                if (idx + 2 < frame.data.size()) {
                    frame.data[idx] = static_cast<uint8_t>((x * 255) / width);      // B/R
                    frame.data[idx + 1] = static_cast<uint8_t>((y * 255) / height); // G
                    frame.data[idx + 2] = static_cast<uint8_t>(128);                 // R/B
                }
            }
        }
    } else if (format == "gray") {
        // Pattern en niveaux de gris
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = y * width + x;
                if (idx < frame.data.size()) {
                    frame.data[idx] = static_cast<uint8_t>((x + y) % 256);
                }
            }
        }
    }
    
    return frame;
}

Frame CreateColorFrame(int width, int height, uint8_t r, uint8_t g, uint8_t b, const std::string& format) {
    Frame frame(width, height, format);
    
    size_t frame_size = CalculateFrameSize(width, height, format);
    frame.data.resize(frame_size);
    
    if (format == "bgr") {
        for (size_t i = 0; i < frame.data.size(); i += 3) {
            frame.data[i] = b;      // B
            frame.data[i + 1] = g;  // G
            frame.data[i + 2] = r;  // R
        }
    } else if (format == "rgb") {
        for (size_t i = 0; i < frame.data.size(); i += 3) {
            frame.data[i] = r;      // R
            frame.data[i + 1] = g;  // G
            frame.data[i + 2] = b;  // B
        }
    } else if (format == "gray") {
        uint8_t gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
        std::fill(frame.data.begin(), frame.data.end(), gray);
    }
    
    return frame;
}

} // namespace FrameUtils
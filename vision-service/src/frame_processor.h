// src/frame_processor.h
#ifndef FRAME_PROCESSOR_H
#define FRAME_PROCESSOR_H

#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <atomic>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

#include "vision.pb.h"

using surveillance::vision::Detection;
using surveillance::vision::BoundingBox;

// Structure pour une frame interne
struct Frame {
    std::vector<uint8_t> data;
    int width;
    int height;
    std::string format;
    std::chrono::steady_clock::time_point timestamp;
    
    Frame() : width(0), height(0), format("unknown") {}
    Frame(int w, int h, const std::string& fmt) 
        : width(w), height(h), format(fmt), timestamp(std::chrono::steady_clock::now()) {}
};

// Résultat du traitement d'une frame
struct ProcessingResult {
    std::vector<Detection> detections;
    int64_t processing_time_ms;
    bool success;
    std::string error_message;
    
    ProcessingResult() : processing_time_ms(0), success(true) {}
};

// Interface pour les détecteurs
class Detector {
public:
    virtual ~Detector() = default;
    virtual std::vector<Detection> Detect(const Frame& frame) = 0;
    virtual std::string GetName() const = 0;
    virtual bool Initialize() = 0;
    virtual void Cleanup() = 0;
};

// Détecteur de mouvement basique (pour Phase 2.1 - simulation)
class BasicMotionDetector : public Detector {
public:
    BasicMotionDetector();
    virtual ~BasicMotionDetector();
    
    std::vector<Detection> Detect(const Frame& frame) override;
    std::string GetName() const override { return "BasicMotionDetector"; }
    bool Initialize() override;
    void Cleanup() override;
    
private:
    bool initialized_;
    Frame previous_frame_;
    std::atomic<int> detection_counter_;
    
    // Paramètres de détection
    double motion_threshold_;
    int min_area_;
    
    // Méthodes privées
    bool HasSignificantChange(const Frame& current, const Frame& previous) const;
    Detection CreateMotionDetection(int x, int y, int width, int height, float confidence) const;
    std::string GenerateDetectionId() const;
};

// Processeur principal de frames
class FrameProcessor {
public:
    FrameProcessor();
    virtual ~FrameProcessor();
    
    // Méthodes principales
    bool Initialize();
    void Cleanup();
    
    ProcessingResult ProcessFrame(const Frame& frame);
    ProcessingResult ProcessFrame(const std::vector<uint8_t>& frame_data, 
                                 int width, int height, 
                                 const std::string& format);
    
    // Gestion des détecteurs
    void AddDetector(std::unique_ptr<Detector> detector);
    void RemoveDetector(const std::string& detector_name);
    std::vector<std::string> GetDetectorNames() const;
    
    // Configuration
    void SetMotionThreshold(double threshold);
    void SetMinDetectionArea(int area);
    void SetMaxDetectionsPerFrame(int max_detections);
    
    // Statistiques
    int64_t GetTotalFramesProcessed() const;
    int64_t GetTotalDetections() const;
    double GetAverageProcessingTime() const;
    
private:
    std::vector<std::unique_ptr<Detector>> detectors_;
    bool initialized_;
    
    // Statistiques
    std::atomic<int64_t> total_frames_processed_{0};
    std::atomic<int64_t> total_detections_{0};
    std::atomic<int64_t> total_processing_time_{0};
    
    // Configuration
    double motion_threshold_;
    int min_detection_area_;
    int max_detections_per_frame_;
    
    // Méthodes privées
    bool ValidateFrame(const Frame& frame) const;
    ProcessingResult CreateErrorResult(const std::string& error) const;
    void UpdateStatistics(int64_t processing_time, int detections_count);
    
#ifdef HAVE_OPENCV
    cv::Mat ConvertToMat(const Frame& frame) const;
    Frame ConvertFromMat(const cv::Mat& mat, const std::string& format = "bgr") const;
#endif
};

// Utilitaires pour la conversion de formats
namespace FrameUtils {
    // Conversion de formats
    std::vector<uint8_t> ConvertFormat(const std::vector<uint8_t>& data,
                                      int width, int height,
                                      const std::string& from_format,
                                      const std::string& to_format);
    
    // Validation des formats
    bool IsValidFormat(const std::string& format);
    std::vector<std::string> GetSupportedFormats();
    
    // Calcul de la taille d'une frame
    size_t CalculateFrameSize(int width, int height, const std::string& format);
    
    // Création de frames de test
    Frame CreateTestFrame(int width = 640, int height = 480, 
                         const std::string& format = "bgr");
    Frame CreateColorFrame(int width, int height, 
                          uint8_t r, uint8_t g, uint8_t b,
                          const std::string& format = "bgr");
}

// Constantes
namespace FrameProcessorConstants {
    constexpr double DEFAULT_MOTION_THRESHOLD = 0.1;
    constexpr int DEFAULT_MIN_AREA = 100;
    constexpr int DEFAULT_MAX_DETECTIONS = 10;
    constexpr int MAX_FRAME_WIDTH = 4096;
    constexpr int MAX_FRAME_HEIGHT = 4096;
    constexpr int MIN_FRAME_WIDTH = 32;
    constexpr int MIN_FRAME_HEIGHT = 32;
    
    // Formats supportés
    const std::vector<std::string> SUPPORTED_FORMATS = {
        "bgr", "rgb", "gray", "jpeg", "png"
    };
}

#endif // FRAME_PROCESSOR_H
#ifndef OPENCV_MOTION_DETECTOR_H
#define OPENCV_MOTION_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include "frame_processor.h"

// Configuration détection de mouvement
struct MotionDetectionConfig {
    double threshold = 25.0;                    // Seuil de détection
    int min_area = 500;                        // Aire minimale (pixels)
    int max_area = 50000;                      // Aire maximale (pixels)
    bool enable_noise_reduction = true;         // Réduction de bruit
    bool enable_shadow_detection = true;       // Détection ombres
    int morphology_size = 3;                   // Taille morphologie
    double learning_rate = 0.005;             // Taux apprentissage background
    int history = 500;                        // Historique background
};

// Détecteur de mouvement avec OpenCV
class OpenCVMotionDetector : public Detector {
public:
    explicit OpenCVMotionDetector(const MotionDetectionConfig& config = MotionDetectionConfig{});
    ~OpenCVMotionDetector() override;
    
    // Interface Detector
    std::vector<Detection> Detect(const Frame& frame) override;
    std::string GetName() const override { return "OpenCVMotionDetector"; }
    bool Initialize() override;
    void Cleanup() override;
    
    // Configuration
    void SetConfig(const MotionDetectionConfig& config);
    MotionDetectionConfig GetConfig() const;
    
    // Debug et visualisation
    cv::Mat GetLastForegroundMask() const;
    cv::Mat GetBackgroundImage() const;
    void EnableDebugOutput(bool enable) { debug_output_ = enable; }
    
private:
    MotionDetectionConfig config_;
    bool initialized_;
    bool debug_output_;
    
    // OpenCV components
    cv::Ptr<cv::BackgroundSubtractorMOG2> bg_subtractor_;
    cv::Mat foreground_mask_;
    cv::Mat background_image_;
    cv::Mat previous_frame_;
    
    // Morphology elements
    cv::Mat morphology_kernel_;
    
    // Statistiques
    std::atomic<uint64_t> total_detections_{0};
    std::atomic<uint64_t> false_positives_filtered_{0};
    
    // Méthodes privées
    std::vector<Detection> ProcessForegroundMask(const cv::Mat& mask, int64_t timestamp);
    bool FilterDetection(const cv::Rect& bbox, double area) const;
    Detection CreateDetectionFromContour(const std::vector<cv::Point>& contour, int64_t timestamp) const;
    void ApplyNoiseReduction(cv::Mat& mask) const;
    std::string GenerateDetectionId() const;
    float CalculateConfidence(const std::vector<cv::Point>& contour, double area, const cv::Rect& bounding_rect) const;
};

#endif // OPENCV_MOTION_DETECTOR_H

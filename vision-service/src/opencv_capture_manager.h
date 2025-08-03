#ifndef OPENCV_CAPTURE_MANAGER_H
#define OPENCV_CAPTURE_MANAGER_H

#include <opencv2/opencv.hpp>
#include "camera_manager.h"

// Gestionnaire de capture spécialisé OpenCV
class OpenCVCaptureManager : public CameraManager {
public:
    explicit OpenCVCaptureManager(const std::string& camera_url);
    ~OpenCVCaptureManager() override;
    
    // Implémentations spécialisées
    bool Initialize(const CameraConfig& config) override;
    void Cleanup() override;
    
protected:
    // Méthodes de capture spécialisées
    Frame CaptureFileFrame() override;
    Frame CaptureWebcamFrame() override; 
    Frame CaptureRtspFrame() override;
    
    // Utilitaires OpenCV
    Frame ConvertMatToFrame(const cv::Mat& mat) const;
    cv::Mat ConvertFrameToMat(const Frame& frame) const;
    
private:
    std::unique_ptr<cv::VideoCapture> opencv_capture_;
    cv::Mat current_frame_;
    
    // Configuration OpenCV spécifique
    bool SetupCapture();
    void OptimizeCapture();
    bool ValidateCapture() const;
};

#endif // OPENCV_CAPTURE_MANAGER_H

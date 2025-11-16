#ifndef OPENCV_CAPTURE_MANAGER_H
#define OPENCV_CAPTURE_MANAGER_H

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#include <memory>
#include <mutex>
#include <atomic>
#include "camera_manager.h"

// Gestionnaire de capture spécialisé OpenCV pour Phase 2.3
// Hérite de CameraManager et fournit une capture vidéo réelle
class OpenCVCaptureManager : public CameraManager {
public:
    explicit OpenCVCaptureManager(const std::string& camera_url);
    ~OpenCVCaptureManager() override;
    
    // Implémentations spécialisées pour capture réelle
    bool Initialize(const CameraConfig& config) override;
    void Cleanup() override;
    bool IsCapturing() const override;
    
    // Statistiques OpenCV spécifiques
    double GetActualFPS() const;
    cv::Size GetFrameSize() const;
    int GetCodec() const;
    
protected:
    // Méthodes de capture spécialisées OpenCV
    Frame CaptureFileFrame() override;
    Frame CaptureWebcamFrame() override; 
    Frame CaptureRtspFrame() override;
    
    // Utilitaires de conversion OpenCV <-> Frame
    Frame ConvertMatToFrame(const cv::Mat& mat) const;
    cv::Mat ConvertFrameToMat(const Frame& frame) const;
    
private:
    // Ressources OpenCV
    std::unique_ptr<cv::VideoCapture> opencv_capture_;
    cv::Mat current_frame_;
    mutable std::mutex capture_mutex_;
    
    // État de capture (is_initialized_ hérité de CameraManager)
    std::atomic<bool> is_capturing_{false};
    
    // Métriques de performance
    mutable std::atomic<double> actual_fps_{0.0};
    cv::Size frame_size_{0, 0};
    int codec_fourcc_{0};
    
    // Configuration et optimisation OpenCV
    bool SetupCapture();
    bool SetupFileCapture();
    bool SetupWebcamCapture();
    bool SetupRtspCapture();
    void OptimizeCapture();
    bool ValidateCapture() const;
    void UpdateMetrics();
    
    // Gestion d'erreurs spécifique OpenCV
    void HandleCaptureError(const std::string& operation) const;
    bool RecoverFromError();
    
    // Paramètres OpenCV spécifiques
    void ApplyOpenCVSettings(const CameraConfig& config);
    void SetBufferSize(int buffer_size);
    void SetTimeoutSettings();
};

#endif // HAVE_OPENCV
#endif // OPENCV_CAPTURE_MANAGER_H

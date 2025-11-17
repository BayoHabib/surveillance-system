#ifndef OPENCV_CAPTURE_MANAGER_H
#define OPENCV_CAPTURE_MANAGER_H

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <queue>
#include <condition_variable>
#include "camera_manager.h"
#include "vision.pb.h"

// Callback pour les événements de détection gRPC
using GrpcDetectionCallback = std::function<bool(const surveillance::vision::DetectionEvent&)>;

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
    bool StartCapture();
    bool StopCapture();
    
    // Statistiques OpenCV spécifiques
    double GetActualFPS() const;
    cv::Size GetFrameSize() const;
    int GetCodec() const;
    
    // Détection de mouvement
    void EnableMotionDetection(bool enable = true);
    bool IsMotionDetectionEnabled() const;
    void SetMotionSensitivity(double sensitivity);
    void SetCameraId(const std::string& camera_id);
    void SetDetectionCallback(GrpcDetectionCallback callback);
    
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
    std::unique_ptr<std::thread> capture_thread_;
    std::atomic<bool> should_stop_capture_{false};
    std::string camera_id_;  // ID de la caméra pour les événements
    std::atomic<int64_t> frame_number_{0};
    
    // Buffer circulaire pour frames (BUG #7 fix)
    std::queue<Frame> frame_buffer_;
    mutable std::mutex buffer_mutex_;  // mutable pour méthodes const
    std::condition_variable buffer_cv_;
    static constexpr size_t MAX_BUFFER_SIZE = 60;  // 2 secondes @ 30 FPS
    std::atomic<int> dropped_frames_{0};
    std::atomic<int> buffer_overflows_{0};
    
    // Callback pour les événements de détection
    GrpcDetectionCallback detection_callback_;
    std::mutex callback_mutex_;
    
    // Détection de mouvement avec MOG2
    cv::Ptr<cv::BackgroundSubtractorMOG2> background_subtractor_;
    cv::Mat foreground_mask_;
    std::atomic<bool> motion_detection_enabled_{false};
    std::atomic<double> motion_sensitivity_{0.7};  // 0.0-1.0
    int motion_detection_threshold_{25};  // Pixels minimum pour détecter
    int motion_detection_blur_{5};  // Taille du blur pour réduire bruit
    std::atomic<int> motion_frames_count_{0};
    
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
    
    // Détection de mouvement
    bool DetectMotion(const cv::Mat& frame);
    void InitializeMotionDetector();
    void ProcessMotionDetection(const cv::Mat& frame);
    int CountMotionPixels(const cv::Mat& mask) const;
    void GenerateDetectionEvent(int motion_pixels);
    
    // Thread de capture
    void CaptureThreadLoop();
    
    // Buffer management (BUG #7 fix)
    void PushFrameToBuffer(Frame&& frame);
    Frame GetNextFrameFromBuffer();
    bool IsBufferEmpty() const;
    size_t GetBufferSize() const;
    
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

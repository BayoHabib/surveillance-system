// src/camera_manager.h
#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <future>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

#include "frame_processor.h"

// Énumération des types de caméras supportés
enum class CameraType {
    UNKNOWN,
    FILE_VIDEO,     // Fichier vidéo (mp4, avi, etc.)
    WEBCAM,         // Webcam locale (/dev/video0, etc.)
    RTSP_STREAM,    // Stream RTSP
    HTTP_STREAM,    // Stream HTTP/MJPEG
    TEST_PATTERN    // Pattern de test généré
};

// Configuration d'une caméra
struct CameraConfig {
    int width = 640;
    int height = 480;
    int fps = 15;
    std::string format = "bgr";
    bool auto_reconnect = true;
    int reconnect_delay_ms = 5000;
    int max_reconnect_attempts = 3;
    int frame_buffer_size = 30;
    
    CameraConfig() = default;
    CameraConfig(int w, int h, int f) : width(w), height(h), fps(f) {}
};

// État d'une caméra
enum class CameraState {
    UNINITIALIZED,
    INITIALIZING,
    READY,
    CAPTURING,
    ERROR,
    DISCONNECTED,
    RECONNECTING
};

// Statistiques d'une caméra
struct CameraStats {
    std::atomic<int64_t> frames_captured{0};
    std::atomic<int64_t> frames_dropped{0};
    std::atomic<int64_t> bytes_received{0};
    std::atomic<int64_t> reconnect_count{0};
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_frame_time;
    
    CameraStats() : start_time(std::chrono::steady_clock::now()) {}
    
    double GetFpsActual() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        return elapsed.count() > 0 ? static_cast<double>(frames_captured.load()) / elapsed.count() : 0.0;
    }
    
    double GetUptimeSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    }
};

// Callback pour les frames capturées
using FrameCallback = std::function<void(const Frame& frame)>;

// Gestionnaire de caméra principal
class CameraManager {
public:
    explicit CameraManager(const std::string& camera_url);
    virtual ~CameraManager();
    
    // Méthodes principales
    bool Initialize();
    bool Initialize(const CameraConfig& config);
    void Cleanup();
    
    bool StartCapture();
    bool StopCapture();
    
    // Configuration
    void SetConfig(const CameraConfig& config);
    CameraConfig GetConfig() const;
    
    // État et statistiques
    CameraState GetState() const;
    const CameraStats& GetStats() const;
    std::string GetLastError() const;
    
    // Callbacks
    void SetFrameCallback(FrameCallback callback);
    void ClearFrameCallback();
    
    // Informations sur la caméra
    std::string GetCameraUrl() const;
    CameraType GetCameraType() const;
    bool IsCapturing() const;
    bool IsConnected() const;
    
    // Méthodes statiques utilitaires
    static CameraType DetectCameraType(const std::string& url);
    static bool IsValidCameraUrl(const std::string& url);
    static std::vector<std::string> GetAvailableWebcams();
    
private:
    std::string camera_url_;
    CameraType camera_type_;
    CameraConfig config_;
    std::atomic<CameraState> state_;
    CameraStats stats_;
    std::string last_error_;
    mutable std::mutex config_mutex_;
    
    // Threading
    std::unique_ptr<std::thread> capture_thread_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> is_capturing_;
    
    // Frame handling
    FrameCallback frame_callback_;
    std::mutex callback_mutex_;
    
    // Buffer de frames
    std::queue<Frame> frame_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_condition_;
    
    // Reconnection
    std::atomic<int> reconnect_attempts_;
    std::chrono::steady_clock::time_point last_reconnect_time_;
    
#ifdef HAVE_OPENCV
    std::unique_ptr<cv::VideoCapture> opencv_capture_;
#endif
    
    // Méthodes privées
    void CaptureLoop();
    bool InitializeCapture();
    bool CaptureFrame();
    void HandleCaptureError(const std::string& error);
    void AttemptReconnect();
    bool ShouldAttemptReconnect() const;
    
    void SetState(CameraState new_state);
    void SetError(const std::string& error);
    void NotifyFrameAvailable(const Frame& frame);
    
    // Implémentations spécifiques par type
    bool InitializeFileCapture();
    bool InitializeWebcamCapture();
    bool InitializeRtspCapture();
    bool InitializeTestPattern();
    
    Frame CaptureFileFrame();
    Frame CaptureWebcamFrame();
    Frame CaptureRtspFrame();
    Frame CaptureTestFrame();
    
    // Utilitaires
    Frame CreateEmptyFrame() const;
    bool ValidateFrame(const Frame& frame) const;
    void UpdateStats(const Frame& frame);
};

// Générateur de pattern de test
class TestPatternGenerator {
public:
    TestPatternGenerator(int width = 640, int height = 480);
    ~TestPatternGenerator() = default;
    
    Frame GenerateColorBars();
    Frame GenerateCheckerboard();
    Frame GenerateMovingBox();
    Frame GenerateNoise();
    Frame GenerateTimeCode();
    
    void SetSize(int width, int height);
    void SetFrameCounter(int counter);
    
private:
    int width_;
    int height_;
    std::atomic<int> frame_counter_{0};
    std::chrono::steady_clock::time_point start_time_;
    
    void DrawText(Frame& frame, const std::string& text, int x, int y);
    void DrawRectangle(Frame& frame, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
    void FillFrame(Frame& frame, uint8_t r, uint8_t g, uint8_t b);
};

// Factory pour créer des CameraManager
class CameraManagerFactory {
public:
    static std::unique_ptr<CameraManager> Create(const std::string& camera_url);
    static std::unique_ptr<CameraManager> Create(const std::string& camera_url, 
                                                const CameraConfig& config);
    
    // Validation et détection
    static bool ValidateUrl(const std::string& url);
    static CameraType DetectType(const std::string& url);
    static std::string GetTypeString(CameraType type);
};

// Constantes et utilitaires
namespace CameraManagerConstants {
    constexpr int DEFAULT_WIDTH = 640;
    constexpr int DEFAULT_HEIGHT = 480;
    constexpr int DEFAULT_FPS = 15;
    constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    constexpr int DEFAULT_RECONNECT_DELAY_MS = 5000;
    constexpr int MAX_FRAME_BUFFER_SIZE = 60;  // 4 secondes à 15fps
    constexpr int CAPTURE_TIMEOUT_MS = 10000;  // 10 secondes
    
    // Types MIME supportés
    const std::vector<std::string> SUPPORTED_VIDEO_FORMATS = {
        ".mp4", ".avi", ".mov", ".mkv", ".wmv", ".flv", ".webm"
    };
    
    // Patterns d'URL supportés
    const std::vector<std::string> RTSP_PREFIXES = {
        "rtsp://", "rtmp://", "rtp://"
    };
    
    const std::vector<std::string> HTTP_PREFIXES = {
        "http://", "https://"
    };
    
    const std::vector<std::string> WEBCAM_PATTERNS = {
        "/dev/video", "/dev/v4l/by-id/"
    };
}

#endif // CAMERA_MANAGER_H
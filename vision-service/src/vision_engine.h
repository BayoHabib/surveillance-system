#ifndef VISION_ENGINE_H
#define VISION_ENGINE_H

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/background_segm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

#include "frame_processor.h"

// Configuration de l'engine
struct VisionEngineConfig {
    int max_streams = 4;
    int default_width = 1920;
    int default_height = 1080;
    int default_fps = 30;
    bool enable_gpu = false;
    bool enable_threading = true;
    int thread_pool_size = 4;
    std::string temp_dir = "/tmp/vision";
};

// Statistiques de performance
struct PerformanceStats {
    std::atomic<uint64_t> frames_processed{0};
    std::atomic<uint64_t> detections_generated{0};
    std::atomic<double> avg_processing_time_ms{0.0};
    std::atomic<double> avg_fps{0.0};
    std::atomic<uint64_t> memory_usage_mb{0};
    std::chrono::steady_clock::time_point start_time;
    
    PerformanceStats() : start_time(std::chrono::steady_clock::now()) {}
};

// Engine principal de vision
class VisionEngine {
public:
    explicit VisionEngine(const VisionEngineConfig& config = VisionEngineConfig{});
    ~VisionEngine();
    
    // Lifecycle
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return initialized_; }
    
    // Stream management
    bool StartStream(const std::string& stream_id, const std::string& source_url);
    bool StopStream(const std::string& stream_id);
    std::vector<std::string> GetActiveStreams() const;
    
    // Configuration
    void SetConfig(const VisionEngineConfig& config);
    VisionEngineConfig GetConfig() const;
    
    // Performance
    PerformanceStats GetPerformanceStats() const;
    void ResetStats();
    
    // Callbacks
    using FrameCallback = std::function<void(const std::string& stream_id, const Frame& frame)>;
    using DetectionCallback = std::function<void(const std::string& stream_id, const std::vector<Detection>& detections)>;
    
    void SetFrameCallback(FrameCallback callback);
    void SetDetectionCallback(DetectionCallback callback);
    
private:
    VisionEngineConfig config_;
    bool initialized_;
    
    // Callbacks
    FrameCallback frame_callback_;
    DetectionCallback detection_callback_;
    
    // Performance tracking
    mutable PerformanceStats stats_;
    mutable std::mutex stats_mutex_;
    
    // Threading
    std::unique_ptr<class ThreadPool> thread_pool_;
    
    // Stream management
    std::map<std::string, std::unique_ptr<class VideoStream>> active_streams_;
    mutable std::mutex streams_mutex_;
    
    // Méthodes privées
    void UpdateStats(double processing_time_ms);
    std::unique_ptr<class VideoStream> CreateStream(const std::string& source_url);
};

#endif // VISION_ENGINE_H

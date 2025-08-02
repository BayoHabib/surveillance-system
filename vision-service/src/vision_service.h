// src/vision_service.h
#ifndef VISION_SERVICE_H
#define VISION_SERVICE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include "vision.grpc.pb.h"
#include "frame_processor.h"
#include "camera_manager.h"

using grpc::Server;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerReaderWriter;

using surveillance::vision::VisionService;
using surveillance::vision::StreamRequest;
using surveillance::vision::StreamResponse;
using surveillance::vision::StopRequest;
using surveillance::vision::StopResponse;
using surveillance::vision::StatusRequest;
using surveillance::vision::StatusResponse;
using surveillance::vision::HealthRequest;
using surveillance::vision::HealthResponse;
using surveillance::vision::FrameRequest;
using surveillance::vision::FrameResponse;

// Structure pour suivre l'état d'un stream
struct StreamState {
    std::string camera_id;
    std::string camera_url;
    std::string status;  // "starting", "active", "stopping", "error"
    std::chrono::steady_clock::time_point start_time;
    std::atomic<int64_t> frames_processed{0};
    std::atomic<int64_t> detections_count{0};
    std::unique_ptr<CameraManager> camera_manager;
    std::unique_ptr<FrameProcessor> frame_processor;
    std::mutex state_mutex;
    
    StreamState(const std::string& cam_id, const std::string& cam_url) 
        : camera_id(cam_id), camera_url(cam_url), status("starting"), 
          start_time(std::chrono::steady_clock::now()) {}
};

// Implémentation du service gRPC
class VisionServiceImpl final : public VisionService::Service {
public:
    VisionServiceImpl();
    virtual ~VisionServiceImpl();
    
    // Méthodes du service gRPC
    Status StartStream(ServerContext* context, 
                      const StreamRequest* request,
                      StreamResponse* response) override;
    
    Status StopStream(ServerContext* context, 
                     const StopRequest* request,
                     StopResponse* response) override;
    
    Status GetStreamStatus(ServerContext* context, 
                          const StatusRequest* request,
                          StatusResponse* response) override;
    
    Status GetHealth(ServerContext* context, 
                    const HealthRequest* request,
                    HealthResponse* response) override;
    
    Status ProcessFrames(ServerContext* context,
                        ServerReaderWriter<FrameResponse, FrameRequest>* stream) override;
    
    // Méthodes utilitaires publiques
    int GetActiveStreamsCount() const;
    
private:
    // État interne
    std::unordered_map<std::string, std::unique_ptr<StreamState>> active_streams_;
    mutable std::mutex streams_mutex_;
    std::chrono::steady_clock::time_point service_start_time_;
    
    // Statistiques
    std::atomic<int64_t> total_streams_started_{0};
    std::atomic<int64_t> total_frames_processed_{0};
    std::atomic<int64_t> total_detections_{0};
    
    // Méthodes privées
    bool IsValidCameraUrl(const std::string& url) const;
    std::string GenerateStreamId(const std::string& camera_id) const;
    void CleanupStream(const std::string& camera_id);
    std::string GetServiceVersion() const;
    
    // Validation des requêtes
    Status ValidateStreamRequest(const StreamRequest* request) const;
    Status ValidateStopRequest(const StopRequest* request) const;
    Status ValidateStatusRequest(const StatusRequest* request) const;
    
    // Gestion des erreurs
    Status CreateErrorResponse(const std::string& message, 
                              const std::string& code = "INTERNAL_ERROR") const;
    
    // Logging helpers
    void LogInfo(const std::string& message) const;
    void LogError(const std::string& message) const;
    void LogDebug(const std::string& message) const;
    
    // Thread safety helpers
    std::unique_lock<std::mutex> LockStreams() const;
    StreamState* GetStreamState(const std::string& camera_id) const;
};

// Classe utilitaire pour les métriques
class ServiceMetrics {
public:
    static ServiceMetrics& Instance();
    
    void IncrementStreamsStarted();
    void IncrementFramesProcessed();
    void IncrementDetections();
    void RecordProcessingTime(int64_t time_ms);
    
    int64_t GetStreamsStarted() const;
    int64_t GetFramesProcessed() const;
    int64_t GetDetections() const;
    double GetAverageProcessingTime() const;
    
private:
    ServiceMetrics() = default;
    
    std::atomic<int64_t> streams_started_{0};
    std::atomic<int64_t> frames_processed_{0};
    std::atomic<int64_t> detections_{0};
    std::atomic<int64_t> total_processing_time_{0};
    std::atomic<int64_t> processing_samples_{0};
};

// Macro pour le logging (simple pour commencer)
#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl
#define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl

// Constantes
namespace VisionServiceConstants {
    constexpr int MAX_CONCURRENT_STREAMS = 10;
    constexpr int DEFAULT_FRAME_BUFFER_SIZE = 30;  // ~2 secondes à 15fps
    constexpr int HEALTH_CHECK_INTERVAL_SEC = 30;
    constexpr int STREAM_TIMEOUT_SEC = 300;  // 5 minutes
    
    // Status codes
    const std::string STATUS_SUCCESS = "success";
    const std::string STATUS_ERROR = "error";
    const std::string STATUS_STARTING = "starting";
    const std::string STATUS_ACTIVE = "active";
    const std::string STATUS_STOPPING = "stopping";
    const std::string STATUS_STOPPED = "stopped";
    
    // Health status
    const std::string HEALTH_HEALTHY = "healthy";
    const std::string HEALTH_DEGRADED = "degraded";
    const std::string HEALTH_UNHEALTHY = "unhealthy";
}

#endif // VISION_SERVICE_H
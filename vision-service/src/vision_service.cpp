// src/vision_service.cpp
#include "vision_service.h"
#include <iostream>
#include <sstream>
#include <regex>

using namespace VisionServiceConstants;

VisionServiceImpl::VisionServiceImpl() 
    : service_start_time_(std::chrono::steady_clock::now()) {
    LogInfo("VisionService initialized");
}

VisionServiceImpl::~VisionServiceImpl() {
    // Nettoyer tous les streams actifs
    auto lock = LockStreams();
    for (auto& [camera_id, stream_state] : active_streams_) {
        LogInfo("Cleaning up stream for camera: " + camera_id);
        CleanupStream(camera_id);
    }
    active_streams_.clear();
    LogInfo("VisionService destroyed");
}

Status VisionServiceImpl::StartStream(ServerContext* context,
                                     const StreamRequest* request,
                                     StreamResponse* response) {
    LogInfo("StartStream called for camera: " + request->camera_id());
    
    // Validation de la requête
    Status validation_status = ValidateStreamRequest(request);
    if (!validation_status.ok()) {
        return validation_status;
    }
    
    const std::string& camera_id = request->camera_id();
    const std::string& camera_url = request->camera_url();
     // After the validation passes, add this debug line:
    auto lock = LockStreams();
    // Vérifier si le stream existe déjà
    if (active_streams_.find(camera_id) != active_streams_.end()) {
        LogError("Stream already exists for camera: " + camera_id);
        response->set_status(STATUS_ERROR);
        response->set_message("Stream already active for camera " + camera_id);
        return Status::OK;
    }
    
    // Vérifier la limite de streams concurrents
    if (active_streams_.size() >= MAX_CONCURRENT_STREAMS) {
        LogError("Maximum concurrent streams reached");
        response->set_status(STATUS_ERROR);
        response->set_message("Maximum number of concurrent streams reached");
        return Status::OK;
    }
    
    try {
        // Créer un nouveau stream state
        auto stream_state = std::make_unique<StreamState>(camera_id, camera_url);
        
        // Créer les composants (pour l'instant, simulés)
        stream_state->camera_manager = std::make_unique<CameraManager>(camera_url);
        stream_state->frame_processor = std::make_unique<FrameProcessor>();
        
        // Initialiser le camera manager
        if (!stream_state->camera_manager->Initialize()) {
            LogError("Failed to initialize camera manager for: " + camera_id);
            response->set_status(STATUS_ERROR);
            response->set_message("Failed to initialize camera for " + camera_id);
            return Status::OK;
        }
        
        // Démarrer le traitement
        if (!stream_state->camera_manager->StartCapture()) {
            LogError("Failed to start capture for: " + camera_id);
            response->set_status(STATUS_ERROR);
            response->set_message("Failed to start capture for " + camera_id);
            return Status::OK;
        }
        
        // Marquer comme actif
        stream_state->status = STATUS_ACTIVE;
        
        // Générer un ID de stream unique
        std::string stream_id = GenerateStreamId(camera_id);
        
        // Ajouter aux streams actifs
        active_streams_[camera_id] = std::move(stream_state);
        
        // Incrémenter les statistiques
        total_streams_started_++;
        ServiceMetrics::Instance().IncrementStreamsStarted();
        
        // Préparer la réponse
        response->set_status(STATUS_SUCCESS);
        response->set_message("Stream started successfully");
        response->set_stream_id(stream_id);
        
        LogInfo("Stream started successfully for camera: " + camera_id);
        
    } catch (const std::exception& e) {
        LogError("Exception in StartStream: " + std::string(e.what()));
        response->set_status(STATUS_ERROR);
        response->set_message("Internal error: " + std::string(e.what()));
    }
    
    return Status::OK;
}

Status VisionServiceImpl::StopStream(ServerContext* context,
                                    const StopRequest* request,
                                    StopResponse* response) {
    LogInfo("StopStream called for camera: " + request->camera_id());
    
    // Validation de la requête
    Status validation_status = ValidateStopRequest(request);
    if (!validation_status.ok()) {
        return validation_status;
    }
    
    const std::string& camera_id = request->camera_id();
    
    auto lock = LockStreams();
    
    auto it = active_streams_.find(camera_id);
    if (it == active_streams_.end()) {
        LogError("Stream not found for camera: " + camera_id);
        response->set_status(STATUS_ERROR);
        response->set_message("No active stream found for camera " + camera_id);
        return Status::OK;
    }
    
    try {
        // Marquer comme en cours d'arrêt
        it->second->status = STATUS_STOPPING;
        
        // Arrêter la capture
        if (it->second->camera_manager) {
            it->second->camera_manager->StopCapture();
        }
        
        // Nettoyer les ressources
        CleanupStream(camera_id);
        
        // Supprimer de la liste
        active_streams_.erase(it);
        
        response->set_status(STATUS_SUCCESS);
        response->set_message("Stream stopped successfully");
        
        LogInfo("Stream stopped successfully for camera: " + camera_id);
        
    } catch (const std::exception& e) {
        LogError("Exception in StopStream: " + std::string(e.what()));
        response->set_status(STATUS_ERROR);
        response->set_message("Internal error: " + std::string(e.what()));
    }
    
    return Status::OK;
}

Status VisionServiceImpl::GetStreamStatus(ServerContext* context,
                                         const StatusRequest* request,
                                         StatusResponse* response) {
    const std::string& camera_id = request->camera_id();
    
    auto lock = LockStreams();
    
    auto it = active_streams_.find(camera_id);
    if (it == active_streams_.end()) {
        response->set_camera_id(camera_id);
        response->set_status(STATUS_STOPPED);
        response->set_message("No active stream");
        return Status::OK;
    }
    
    const auto& stream_state = it->second;
    
    // Calculer l'uptime
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - stream_state->start_time
    ).count();
    
    // Calculer le FPS approximatif
    double fps_actual = 0.0;
    if (uptime > 0) {
        fps_actual = static_cast<double>(stream_state->frames_processed.load()) / uptime;
    }
    
    // Remplir la réponse
    response->set_camera_id(camera_id);
    response->set_status(stream_state->status);
    response->set_message("Stream active");
    
    // Statistiques
    auto* stats = response->mutable_stats();
    stats->set_frames_processed(stream_state->frames_processed.load());
    stats->set_detections_count(stream_state->detections_count.load());
    stats->set_fps_actual(fps_actual);
    stats->set_uptime_seconds(uptime);
    stats->set_last_frame_timestamp(
        std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()
        ).count()
    );
    
    return Status::OK;
}

Status VisionServiceImpl::GetHealth(ServerContext* context,
                                   const HealthRequest* request,
                                   HealthResponse* response) {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - service_start_time_
    ).count();
    
    int active_streams_count = 0;
    std::string health_status = HEALTH_HEALTHY;
    std::string health_message = "Service is healthy";
    
    {
        auto lock = LockStreams();
        active_streams_count = active_streams_.size();
        
        // Vérifier s'il y a des streams en erreur
        for (const auto& [camera_id, stream_state] : active_streams_) {
            if (stream_state->status == "error") {
                health_status = HEALTH_DEGRADED;
                health_message = "One or more streams in error state";
                break;
            }
        }
    }
    
    // Si trop de streams actifs, marquer comme dégradé
    if (active_streams_count >= MAX_CONCURRENT_STREAMS * 0.9) {
        health_status = HEALTH_DEGRADED;
        health_message = "Approaching maximum concurrent streams";
    }
    
    response->set_status(health_status);
    response->set_message(health_message);
    response->set_active_streams(active_streams_count);
    response->set_uptime_seconds(uptime);
    response->set_version(GetServiceVersion());
    
    return Status::OK;
}

Status VisionServiceImpl::ProcessFrames(ServerContext* context,
                                       ServerReaderWriter<FrameResponse, FrameRequest>* stream) {
    LogInfo("ProcessFrames stream started");
    
    FrameRequest request;
    while (stream->Read(&request)) {
        const std::string& camera_id = request.camera_id();
        
        // Traitement simulé pour Phase 2.1
        FrameResponse response;
        response.set_camera_id(camera_id);
        response.set_timestamp(request.timestamp());
        
        // Simuler quelques détections
        // (sera remplacé par du vrai traitement OpenCV en Phase 2.3)
        
        // Statistiques de traitement simulées
        auto* stats = response.mutable_processing_stats();
        stats->set_processing_time_ms(10);  // 10ms simulé
        stats->set_detections_count(0);     // Aucune détection pour l'instant
        stats->set_cpu_usage(15.5f);        // 15.5% simulé
        stats->set_memory_usage_mb(128);    // 128MB simulé
        
        if (!stream->Write(response)) {
            LogError("Failed to write frame response");
            break;
        }
        
        // Incrémenter les statistiques
        total_frames_processed_++;
        ServiceMetrics::Instance().IncrementFramesProcessed();
    }
    
    LogInfo("ProcessFrames stream ended");
    return Status::OK;
}

int VisionServiceImpl::GetActiveStreamsCount() const {
    auto lock = LockStreams();
    return active_streams_.size();
}

// Méthodes privées

// In src/vision_service.cpp, replace the IsValidCameraUrl method:

bool VisionServiceImpl::IsValidCameraUrl(const std::string& url) const {
    if (url.empty()) {
        return false;
    }
    
    // Delegate to CameraManager's validation logic
    return CameraManager::IsValidCameraUrl(url);
}

std::string VisionServiceImpl::GenerateStreamId(const std::string& camera_id) const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    return camera_id + "_" + std::to_string(timestamp);
}

void VisionServiceImpl::CleanupStream(const std::string& camera_id) {
    // Nettoyage des ressources (pour l'instant minimal)
    LogDebug("Cleaning up resources for camera: " + camera_id);
    // Les unique_ptr se nettoient automatiquement
}

std::string VisionServiceImpl::GetServiceVersion() const {
    return "1.0.0-phase2.1";
}

Status VisionServiceImpl::ValidateStreamRequest(const StreamRequest* request) const {
    if (request->camera_id().empty()) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Camera ID cannot be empty");
    }
    
    if (request->camera_url().empty()) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Camera URL cannot be empty");
    }
    
    if (!IsValidCameraUrl(request->camera_url())) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid camera URL format");
    }
    
    return Status::OK;
}

Status VisionServiceImpl::ValidateStopRequest(const StopRequest* request) const {
    if (request->camera_id().empty()) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Camera ID cannot be empty");
    }
    
    return Status::OK;
}

Status VisionServiceImpl::ValidateStatusRequest(const StatusRequest* request) const {
    if (request->camera_id().empty()) {
        return Status(grpc::StatusCode::INVALID_ARGUMENT, "Camera ID cannot be empty");
    }
    
    return Status::OK;
}

void VisionServiceImpl::LogInfo(const std::string& message) const {
    LOG_INFO(message);
}

void VisionServiceImpl::LogError(const std::string& message) const {
    LOG_ERROR(message);
}

void VisionServiceImpl::LogDebug(const std::string& message) const {
    LOG_DEBUG(message);
}

std::unique_lock<std::mutex> VisionServiceImpl::LockStreams() const {
    return std::unique_lock<std::mutex>(streams_mutex_);
}

StreamState* VisionServiceImpl::GetStreamState(const std::string& camera_id) const {
    auto it = active_streams_.find(camera_id);
    return (it != active_streams_.end()) ? it->second.get() : nullptr;
}


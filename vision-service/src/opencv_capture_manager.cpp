// src/opencv_capture_manager.cpp
// Implémentation du gestionnaire de capture avec OpenCV

#include "opencv_capture_manager.h"
#include "logger.h"
#include <iostream>
#include <algorithm>
#include <chrono>

using namespace cv;

OpenCVCaptureManager::OpenCVCaptureManager(const std::string& camera_url)
    : CameraManager(camera_url), opencv_capture_(nullptr),
      last_detection_time_(std::chrono::steady_clock::now() - std::chrono::seconds(DETECTION_COOLDOWN_SECONDS)) {
    LOG_DEBUG("[OpenCVCaptureManager] Constructed with URL: ", camera_url);
}

OpenCVCaptureManager::~OpenCVCaptureManager() {
    try {
        Cleanup();
    } catch (const std::exception& e) {
        LOG_ERROR("[OpenCVCaptureManager] Exception in destructor: ", e.what());
    }
}

bool OpenCVCaptureManager::Initialize(const CameraConfig& config) {
    LOG_INFO("[OpenCVCaptureManager] Initializing camera: ", camera_url_);
    
    // Configuration de base
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (state_ != CameraState::UNINITIALIZED) {
            SetError("Already initialized");
            return false;
        }
        SetState(CameraState::INITIALIZING);
        config_ = config;
    }

    try {
        if (!SetupCapture()) {
            SetError("Failed to setup OpenCV capture");
            SetState(CameraState::ERROR);
            return false;
        }

        // Optimiser la configuration
        OptimizeCapture();
        
        // Valider la configuration
        if (!ValidateCapture()) {
            SetError("OpenCV capture validation failed");
            SetState(CameraState::ERROR);
            return false;
        }

        SetState(CameraState::READY);
        CameraManager::is_initialized_ = true;  // Explicitement le membre de la classe de base
        stats_.start_time = std::chrono::steady_clock::now();
        
        // Activer la détection de mouvement par défaut
        EnableMotionDetection(true);
        SetMotionSensitivity(0.3);  // Sensibilité basse pour éviter les faux positifs sur vidéos
        
        std::cerr << "[OpenCVCaptureManager] OpenCV initialization successful" << std::endl;
        return true;

    } catch (const std::exception& e) {
        SetError("OpenCV initialization exception: " + std::string(e.what()));
        SetState(CameraState::ERROR);
        return false;
    }
}

void OpenCVCaptureManager::Cleanup() {
    std::cerr << "[OpenCVCaptureManager] Cleanup with OpenCV" << std::endl;
    
    // Arrêter la capture d'abord
    StopCapture();
    
    // Nettoyer les ressources OpenCV
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (opencv_capture_) {
            opencv_capture_->release();
            opencv_capture_.reset();
        }
        current_frame_.release();
    }
    
    // Appeler le cleanup de base
    CameraManager::Cleanup();
}

bool OpenCVCaptureManager::SetupCapture() {
    std::cerr << "[OpenCVCaptureManager] Setting up capture for type: " 
              << static_cast<int>(camera_type_) << std::endl;

    opencv_capture_ = std::make_unique<VideoCapture>();

    switch (camera_type_) {
        case CameraType::FILE_VIDEO:
            return SetupFileCapture();
        case CameraType::WEBCAM:
            return SetupWebcamCapture();
        case CameraType::RTSP_STREAM:
            return SetupRtspCapture();
        case CameraType::TEST_PATTERN:
            // Maintenir la compatibilité avec les patterns de test
            return true;
        default:
            SetError("Unsupported camera type for OpenCV");
            return false;
    }
}

bool OpenCVCaptureManager::SetupFileCapture() {
    // Remove file:// prefix if present
    std::string file_path = camera_url_;
    if (file_path.find("file://") == 0) {
        file_path = file_path.substr(7); // Remove "file://"
        std::cerr << "[OpenCVCaptureManager] Cleaned URL from " << camera_url_ << " to " << file_path << std::endl;
    }
    
    std::cerr << "[OpenCVCaptureManager] Setting up file capture: " << file_path << std::endl;
    
    if (!opencv_capture_->open(file_path)) {
        SetError("Failed to open video file: " + file_path);
        return false;
    }

    // Obtenir les propriétés du fichier
    double fps = opencv_capture_->get(CAP_PROP_FPS);
    int width = static_cast<int>(opencv_capture_->get(CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(opencv_capture_->get(CAP_PROP_FRAME_HEIGHT));
    int frame_count = static_cast<int>(opencv_capture_->get(CAP_PROP_FRAME_COUNT));

    std::cerr << "[OpenCVCaptureManager] File properties: " 
              << width << "x" << height << " @ " << fps << "fps, " 
              << frame_count << " frames" << std::endl;

    // Ajuster la configuration si nécessaire
    if (config_.fps <= 0) {
        config_.fps = static_cast<int>(fps);
    }
    if (config_.width <= 0) {
        config_.width = width;
    }
    if (config_.height <= 0) {
        config_.height = height;
    }

    return true;
}

bool OpenCVCaptureManager::SetupWebcamCapture() {
    std::cerr << "[OpenCVCaptureManager] Setting up webcam capture: " << camera_url_ << std::endl;
    
    // Extraire l'index de la webcam
    int device_index = 0;
    if (camera_url_.find("/dev/video") == 0) {
        try {
            device_index = std::stoi(camera_url_.substr(10)); // "/dev/video".length() = 10
        } catch (const std::exception& e) {
            SetError("Invalid webcam device index: " + camera_url_);
            return false;
        }
    }

    // Essayer d'ouvrir avec différents backends
    std::vector<int> backends = {CAP_V4L2, CAP_ANY};
    
    for (int backend : backends) {
        if (opencv_capture_->open(device_index, backend)) {
            std::cerr << "[OpenCVCaptureManager] Webcam opened with backend: " << backend << std::endl;
            break;
        }
    }

    if (!opencv_capture_->isOpened()) {
        SetError("Failed to open webcam: " + camera_url_);
        return false;
    }

    // Configurer la webcam
    opencv_capture_->set(CAP_PROP_FRAME_WIDTH, config_.width);
    opencv_capture_->set(CAP_PROP_FRAME_HEIGHT, config_.height);
    opencv_capture_->set(CAP_PROP_FPS, config_.fps);

    // Vérifier les propriétés réelles
    int actual_width = static_cast<int>(opencv_capture_->get(CAP_PROP_FRAME_WIDTH));
    int actual_height = static_cast<int>(opencv_capture_->get(CAP_PROP_FRAME_HEIGHT));
    double actual_fps = opencv_capture_->get(CAP_PROP_FPS);

    std::cerr << "[OpenCVCaptureManager] Webcam configured: " 
              << actual_width << "x" << actual_height << " @ " << actual_fps << "fps" << std::endl;

    return true;
}

bool OpenCVCaptureManager::SetupRtspCapture() {
    LOG_INFO("[OpenCVCaptureManager] Setting up RTSP: ", camera_url_);
    
    // Try FFMPEG backend first (better RTSP support than GStreamer)
    LOG_DEBUG("[OpenCVCaptureManager] Trying FFMPEG backend...");
    opencv_capture_ = std::make_unique<cv::VideoCapture>(camera_url_, CAP_FFMPEG);
    
    // Configuration spéciale pour RTSP
    opencv_capture_->set(CAP_PROP_BUFFERSIZE, 1); // Réduire la latence
    opencv_capture_->set(CAP_PROP_OPEN_TIMEOUT_MSEC, 10000);  // 10s timeout
    opencv_capture_->set(CAP_PROP_READ_TIMEOUT_MSEC, 5000);   // 5s read timeout
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (!opencv_capture_->isOpened()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        LOG_WARN("[OpenCVCaptureManager] FFMPEG failed after ", elapsed, "s, trying default backend");
        
        // Fallback to default backend
        opencv_capture_ = std::make_unique<cv::VideoCapture>();
        opencv_capture_->set(CAP_PROP_BUFFERSIZE, 1);
        if (!opencv_capture_->open(camera_url_)) {
            SetError("Failed to open RTSP stream: " + camera_url_);
            return false;
        }
    }
    
    auto open_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    LOG_INFO("[OpenCVCaptureManager] RTSP stream opened in ", open_elapsed, "ms");

    // RTSP peut prendre du temps à se connecter - validate with test frame
    Mat test_frame;
    int attempts = 0;
    const int max_attempts = 10;
    
    while (attempts < max_attempts) {
        bool read_success = opencv_capture_->read(test_frame);
                  
        if (read_success && !test_frame.empty()) {
            LOG_INFO("[OpenCVCaptureManager] RTSP validated - ", 
                      test_frame.cols, "x", test_frame.rows);
            return true;
        }
        
        // Only log every 3 attempts to reduce verbosity
        if (attempts % 3 == 0) {
            LOG_DEBUG("[OpenCVCaptureManager] Waiting for RTSP frames... attempt ", 
                     (attempts + 1), "/", max_attempts);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        attempts++;
    }

    LOG_ERROR("[OpenCVCaptureManager] RTSP timeout - no frames after ", max_attempts, " attempts");
    SetError("RTSP stream timeout - no frames received");
    return false;
}

void OpenCVCaptureManager::OptimizeCapture() {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        return;
    }

    LOG_DEBUG("[OpenCVCaptureManager] Optimizing capture settings...");

    // Optimisations générales
    opencv_capture_->set(CAP_PROP_BUFFERSIZE, 1); // Réduire latence
    
    // Optimisations spécifiques selon le type
    switch (camera_type_) {
        case CameraType::WEBCAM:
            // Optimisations webcam
            opencv_capture_->set(CAP_PROP_AUTO_EXPOSURE, 0.25); // Exposition manuelle
            opencv_capture_->set(CAP_PROP_AUTOFOCUS, 0);        // Focus manuel
            break;
            
        case CameraType::RTSP_STREAM:
            // Optimisations RTSP
            opencv_capture_->set(CAP_PROP_OPEN_TIMEOUT_MSEC, 10000);  // 10s timeout
            opencv_capture_->set(CAP_PROP_READ_TIMEOUT_MSEC, 5000);   // 5s read timeout
            break;
            
        default:
            break;
    }

    LOG_DEBUG("[OpenCVCaptureManager] Capture optimization completed");
}

bool OpenCVCaptureManager::ValidateCapture() const {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        return false;
    }

    // Test de lecture d'une frame
    Mat test_frame;
    if (!opencv_capture_->read(test_frame) || test_frame.empty()) {
        return false;
    }

    // Vérifier les dimensions
    if (test_frame.cols <= 0 || test_frame.rows <= 0) {
        return false;
    }

    LOG_INFO("[OpenCVCaptureManager] Capture validation successful: ", 
              test_frame.cols, "x", test_frame.rows);

    return true;
}

Frame OpenCVCaptureManager::CaptureFileFrame() {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        LOG_ERROR("[OpenCVCaptureManager] File capture not initialized");
        return CreateEmptyFrame();
    }

    Mat frame;
    if (!opencv_capture_->read(frame)) {
        // Fin de fichier ou erreur
        LOG_DEBUG("[OpenCVCaptureManager] End of file or read error");
        
        // Option: boucler le fichier
        if (camera_type_ == CameraType::FILE_VIDEO) {
            opencv_capture_->set(CAP_PROP_POS_FRAMES, 0); // Revenir au début
            if (opencv_capture_->read(frame)) {
                LOG_DEBUG("[OpenCVCaptureManager] File looped successfully");
                // Appliquer détection de mouvement si activée
                if (!frame.empty()) {
                    ProcessMotionDetection(frame);
                }
                return ConvertMatToFrame(frame);
            }
        }
        return CreateEmptyFrame();
    }

    // Appliquer détection de mouvement si activée
    if (!frame.empty()) {
        ProcessMotionDetection(frame);
    }

    return ConvertMatToFrame(frame);
}

Frame OpenCVCaptureManager::CaptureWebcamFrame() {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        LOG_ERROR("[OpenCVCaptureManager] Webcam capture not initialized");
        return CreateEmptyFrame();
    }

    Mat frame;
    if (!opencv_capture_->read(frame)) {
        LOG_ERROR("[OpenCVCaptureManager] Webcam read failed");
        return CreateEmptyFrame();
    }

    if (frame.empty()) {
        LOG_WARN("[OpenCVCaptureManager] Webcam returned empty frame");
        return CreateEmptyFrame();
    }

    // Appliquer détection de mouvement si activée
    ProcessMotionDetection(frame);

    return ConvertMatToFrame(frame);
}

Frame OpenCVCaptureManager::CaptureRtspFrame() {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        LOG_ERROR("[OpenCVCaptureManager] RTSP capture not initialized");
        return CreateEmptyFrame();
    }

    Mat frame;
    
    // BUG #8 FIX: Retry loop avec exponential backoff
    bool frame_captured = opencv_capture_->read(frame);
    
    while (!frame_captured) {
        LOG_WARN("[OpenCVCaptureManager] RTSP read failed, attempting reconnection...");
        
        // Vérifier si on peut tenter une reconnection (avec backoff)
        if (!ShouldAttemptReconnect()) {
            // Max tentatives atteintes ou backoff delay non écoulé
            return CreateEmptyFrame();
        }
        
        // Tenter la reconnection
        opencv_capture_->release();
        
        // Le backoff delay est déjà géré par ShouldAttemptReconnect()
        // On applique juste un petit sleep additionnel pour stabiliser
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (opencv_capture_->open(camera_url_)) {
            LOG_INFO("[OpenCVCaptureManager] RTSP reconnection successful!");
            
            // Réessayer la capture
            frame_captured = opencv_capture_->read(frame);
            
            if (frame_captured) {
                // Succès! Reset le compteur de tentatives
                ResetReconnectAttempts();
                break;
            } else {
                LOG_WARN("[OpenCVCaptureManager] Frame read failed after reconnection");
                // Continue loop pour réessayer avec backoff plus long
            }
        } else {
            LOG_ERROR("[OpenCVCaptureManager] RTSP reconnection failed");
            // Continue loop pour réessayer avec backoff plus long
        }
    }
    
    if (!frame_captured || frame.empty()) {
        return CreateEmptyFrame();
    }

    // Appliquer détection de mouvement si activée
    if (!frame.empty()) {
        ProcessMotionDetection(frame);
    }

    return ConvertMatToFrame(frame);
}

Frame OpenCVCaptureManager::ConvertMatToFrame(const Mat& mat) const {
    if (mat.empty()) {
        return CreateEmptyFrame();
    }

    Frame frame(mat.cols, mat.rows, "bgr");
    
    // Copier les données
    size_t data_size = mat.total() * mat.elemSize();
    frame.data.resize(data_size);
    std::memcpy(frame.data.data(), mat.data, data_size);
    
    // Remplir les métadonnées
    frame.timestamp = std::chrono::steady_clock::now();
    frame.channels = mat.channels();
    frame.frame_number = frame_number_.load();
    frame.has_motion = false;  // Sera mis à jour si détection
    
    return frame;
}

Mat OpenCVCaptureManager::ConvertFrameToMat(const Frame& frame) const {
    if (frame.data.empty() || frame.width <= 0 || frame.height <= 0) {
        return Mat();
    }

    int cv_type = CV_8UC3; // Par défaut BGR
    if (frame.format == "gray") {
        cv_type = CV_8UC1;
    } else if (frame.format == "rgba") {
        cv_type = CV_8UC4;
    }

    Mat mat(frame.height, frame.width, cv_type);
    
    // Vérifier la taille des données
    size_t expected_size = mat.total() * mat.elemSize();
    if (frame.data.size() >= expected_size) {
        std::memcpy(mat.data, frame.data.data(), expected_size);
    } else {
        LOG_ERROR("[OpenCVCaptureManager] Frame data size mismatch: expected ", 
                  expected_size, ", got ", frame.data.size());
        return Mat();
    }

    return mat;
}

bool OpenCVCaptureManager::IsCapturing() const {
    return opencv_capture_ && opencv_capture_->isOpened() && is_capturing_.load();
}

bool OpenCVCaptureManager::StartCapture() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!CameraManager::is_initialized_) {
        LOG_ERROR("Cannot start OpenCV capture: not initialized");
        return false;
    }
    
    if (is_capturing_.load()) {
        LOG_WARNING("OpenCV capture already active");
        return true;
    }
    
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        LOG_ERROR("OpenCV VideoCapture not ready");
        return false;
    }
    
    // Démarrer le thread de capture
    should_stop_capture_.store(false);
    is_capturing_.store(true);
    capture_active_ = true;
    
    capture_thread_ = std::make_unique<std::thread>(
        &OpenCVCaptureManager::CaptureThreadLoop, this
    );
    
    SetState(CameraState::CAPTURING);
    LOG_INFO("[OpenCVCaptureManager] OpenCV capture thread started with motion detection");
    
    return true;
}

bool OpenCVCaptureManager::StopCapture() {
    LOG_INFO("[OpenCVCaptureManager] Stopping OpenCV capture");
    
    should_stop_capture_.store(true);
    is_capturing_.store(false);
    capture_active_ = false;
    
    // Attendre la fin du thread
    if (capture_thread_ && capture_thread_->joinable()) {
        capture_thread_->join();
        capture_thread_.reset();
        LOG_DEBUG("[OpenCVCaptureManager] Capture thread stopped");
    }
    
    SetState(CameraState::READY);
    
    return true;
}

void OpenCVCaptureManager::CaptureThreadLoop() {
    LOG_DEBUG("[OpenCVCaptureManager] Capture thread loop started");
    
    int frame_count = 0;
    auto last_log_time = std::chrono::steady_clock::now();
    
    while (!should_stop_capture_.load()) {
        try {
            // Capturer une frame selon le type de source
            Frame frame;
            switch (camera_type_) {
                case CameraType::FILE_VIDEO:
                    frame = CaptureFileFrame();
                    break;
                case CameraType::WEBCAM:
                    frame = CaptureWebcamFrame();
                    break;
                case CameraType::RTSP_STREAM:
                    frame = CaptureRtspFrame();
                    break;
                default:
                    LOG_ERROR("[OpenCVCaptureManager] Unsupported camera type");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
            }
            
            if (frame.data.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps
                continue;
            }
            
            // Incrémenter compteur
            frame_count++;
            total_frames_captured_++;
            stats_.frames_captured++;
            frame_number_++;
            
            // Mettre à jour frame_number
            frame.frame_number = frame_number_.load();
            
            // Détecter le mouvement si activé
            if (motion_detection_enabled_.load()) {
                cv::Mat mat = ConvertFrameToMat(frame);
                frame.has_motion = DetectMotion(mat);
            }
            
            // NOUVEAU: Pousser dans buffer au lieu de callback direct
            PushFrameToBuffer(std::move(frame));
            
            // Log périodique réduit (toutes les 10 secondes au lieu de 5)
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
            if (elapsed.count() >= 10) {
                double fps = frame_count / static_cast<double>(elapsed.count());
                size_t buffer_size = GetBufferSize();
                LOG_INFO("[OpenCVCaptureManager] Stats: ", frame_count, " frames in ", 
                         elapsed.count(), "s (", static_cast<int>(fps), " FPS) | Buffer: ", 
                         buffer_size, "/", MAX_BUFFER_SIZE, " | Dropped: ", 
                         dropped_frames_.load(), " | Motion: ", motion_frames_count_.load());
                frame_count = 0;
                last_log_time = now;
            }
            
            // Contrôler le framerate (environ 30 FPS max)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            
        } catch (const std::exception& e) {
            LOG_ERROR("[OpenCVCaptureManager] Exception in capture loop: ", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LOG_DEBUG("[OpenCVCaptureManager] Capture thread loop ended");
}

// ============================================================================
// Buffer Circulaire Management (BUG #7 Fix)
// ============================================================================

void OpenCVCaptureManager::PushFrameToBuffer(Frame&& frame) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // Si buffer plein, supprimer la frame la plus ancienne
    if (frame_buffer_.size() >= MAX_BUFFER_SIZE) {
        frame_buffer_.pop();
        dropped_frames_++;
        buffer_overflows_++;
        
        // Log réduit: toutes les 500 overflows au lieu de 100
        if (buffer_overflows_.load() % 500 == 0) {
            LOG_WARN("[OpenCVCaptureManager] Buffer overflow! Total dropped frames: ", 
                     dropped_frames_.load());
        }
    }
    
    frame_buffer_.push(std::move(frame));
    buffer_cv_.notify_one();
}

Frame OpenCVCaptureManager::GetNextFrameFromBuffer() {
    std::unique_lock<std::mutex> lock(buffer_mutex_);
    
    // Attendre qu'une frame soit disponible (max 5 secondes)
    if (!buffer_cv_.wait_for(lock, std::chrono::seconds(5), 
                              [this] { return !frame_buffer_.empty() || should_stop_capture_.load(); })) {
        LOG_WARN("[OpenCVCaptureManager] Timeout waiting for frame in buffer");
        return CreateEmptyFrame();
    }
    
    if (should_stop_capture_.load() && frame_buffer_.empty()) {
        return CreateEmptyFrame();
    }
    
    Frame frame = std::move(frame_buffer_.front());
    frame_buffer_.pop();
    return frame;
}

bool OpenCVCaptureManager::IsBufferEmpty() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return frame_buffer_.empty();
}

size_t OpenCVCaptureManager::GetBufferSize() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return frame_buffer_.size();
}

// ============================================================================
// Détection de Mouvement avec BackgroundSubtractorMOG2
// ============================================================================

void OpenCVCaptureManager::EnableMotionDetection(bool enable) {
    motion_detection_enabled_.store(enable);
    if (enable && !background_subtractor_) {
        InitializeMotionDetector();
    }
    LOG_INFO("[OpenCVCaptureManager] Motion detection ", 
              (enable ? "enabled" : "disabled"));
}

bool OpenCVCaptureManager::IsMotionDetectionEnabled() const {
    return motion_detection_enabled_.load();
}

void OpenCVCaptureManager::SetMotionSensitivity(double sensitivity) {
    if (sensitivity >= 0.0 && sensitivity <= 1.0) {
        motion_sensitivity_.store(sensitivity);
        // Ajuster le seuil : plus sensible = seuil plus bas
        // Pour 1280x720 (921,600 pixels totaux):
        // - sensibilité 0.1 (très basse) = 45,000 pixels (4.9% de l'image)
        // - sensibilité 0.3 (basse) = 35,000 pixels (3.8% de l'image)
        // - sensibilité 0.5 (moyenne) = 25,000 pixels (2.7% de l'image)
        // - sensibilité 0.7 (haute) = 15,000 pixels (1.6% de l'image)
        // - sensibilité 0.9 (très haute) = 5,000 pixels (0.5% de l'image)
        motion_detection_threshold_ = static_cast<int>(50000 * (1.0 - sensitivity));
        LOG_INFO("[OpenCVCaptureManager] Motion sensitivity set to ", 
                  sensitivity, " (threshold: ", motion_detection_threshold_, " pixels)");
    }
}

void OpenCVCaptureManager::SetCameraId(const std::string& camera_id) {
    camera_id_ = camera_id;
    LOG_DEBUG("[OpenCVCaptureManager] Camera ID set to: ", camera_id);
}

void OpenCVCaptureManager::SetDetectionCallback(GrpcDetectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    detection_callback_ = callback;
    if (callback) {
        LOG_DEBUG("[OpenCVCaptureManager] Detection callback registered for camera: ", camera_id_);
    } else {
        LOG_DEBUG("[OpenCVCaptureManager] Detection callback cleared for camera: ", camera_id_);
    }
}

void OpenCVCaptureManager::InitializeMotionDetector() {
    LOG_DEBUG("[OpenCVCaptureManager] Initializing MOG2 background subtractor");
    
    // Créer le détecteur MOG2 avec paramètres optimisés
    int history = 500;  // Nombre de frames pour l'historique
    double varThreshold = 16;  // Seuil de variance (plus bas = plus sensible)
    bool detectShadows = true;  // Détecter et ignorer les ombres
    
    background_subtractor_ = cv::createBackgroundSubtractorMOG2(
        history, 
        varThreshold, 
        detectShadows
    );
    
    if (detectShadows) {
        // Valeur des pixels d'ombre (127 en grayscale)
        background_subtractor_->setShadowValue(0);
        background_subtractor_->setShadowThreshold(0.5);
    }
    
    // Paramètres supplémentaires
    background_subtractor_->setNMixtures(5);  // Nombre de gaussiennes
    background_subtractor_->setBackgroundRatio(0.9);
    background_subtractor_->setComplexityReductionThreshold(0.05);
    
    LOG_DEBUG("[OpenCVCaptureManager] MOG2 initialized with history=", history, 
              ", varThreshold=", varThreshold);
}

bool OpenCVCaptureManager::DetectMotion(const cv::Mat& frame) {
    if (!motion_detection_enabled_.load() || frame.empty()) {
        return false;
    }
    
    if (!background_subtractor_) {
        InitializeMotionDetector();
    }
    
    try {
        // Appliquer le background subtractor
        background_subtractor_->apply(frame, foreground_mask_);
        
        // Réduire le bruit avec un blur
        if (motion_detection_blur_ > 0) {
            cv::GaussianBlur(foreground_mask_, foreground_mask_, 
                            cv::Size(motion_detection_blur_, motion_detection_blur_), 0);
        }
        
        // Binariser le masque
        cv::threshold(foreground_mask_, foreground_mask_, 127, 255, cv::THRESH_BINARY);
        
        // Compter les pixels de mouvement
        int motion_pixels = CountMotionPixels(foreground_mask_);
        
        // Détecter si mouvement significatif
        bool motion_detected = motion_pixels > motion_detection_threshold_;
        
        if (motion_detected) {
            // BUG #3 FIX: Vérifier le cooldown avant de générer un événement
            std::lock_guard<std::mutex> lock(detection_mutex_);
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_detection_time_).count();
            
            if (elapsed >= DETECTION_COOLDOWN_SECONDS) {
                motion_frames_count_++;
                LOG_INFO("[OpenCVCaptureManager] Motion detected! Pixels: ", 
                          motion_pixels, " (threshold: ", motion_detection_threshold_, 
                          "), frame #", motion_frames_count_.load(), 
                          " [cooldown: ", elapsed, "s]");
                
                // Générer un événement de détection
                GenerateDetectionEvent(motion_pixels);
                
                // Mettre à jour le timestamp de la dernière détection
                last_detection_time_ = now;
                
                return true;
            } else {
                // Détection ignorée (cooldown actif)
                // Pas de log pour éviter spam console
                return false;
            }
        }
        
        return false;
        
    } catch (const cv::Exception& e) {
        LOG_ERROR("[OpenCVCaptureManager] OpenCV exception in motion detection: ", 
                  e.what());
        return false;
    }
}

void OpenCVCaptureManager::ProcessMotionDetection(const cv::Mat& frame) {
    if (DetectMotion(frame)) {
        // Motion détecté - notifier via callback si disponible
        // Le frame avec mouvement sera traité par le FrameProcessor
    }
}

int OpenCVCaptureManager::CountMotionPixels(const cv::Mat& mask) const {
    if (mask.empty()) {
        return 0;
    }
    return cv::countNonZero(mask);
}

void OpenCVCaptureManager::GenerateDetectionEvent(int motion_pixels) {
    // Créer l'événement proto
    surveillance::vision::DetectionEvent event;
    event.set_camera_id(camera_id_);
    event.set_detection_id(camera_id_ + "_" + std::to_string(frame_number_.load()));
    event.set_frame_number(frame_number_.load());
    event.set_motion_pixels(motion_pixels);
    
    // Créer une détection basique
    auto* detection = event.mutable_detection();
    detection->set_type("motion");
    detection->set_confidence(motion_pixels > 5000 ? 0.9f : 0.7f);
    detection->set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    // Appeler le callback si disponible
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (detection_callback_) {
            if (!detection_callback_(event)) {
                LOG_WARN("[OpenCVCaptureManager] Detection callback returned false, stopping notifications");
                detection_callback_ = nullptr;
            }
        }
    }
    
    // Log l'événement réduit (seulement en DEBUG)
    LOG_DEBUG("[OpenCVCaptureManager] Detection event sent: camera=", camera_id_, 
              ", pixels=", motion_pixels, ", frame=", frame_number_.load());
}

// BUG #8 FIX: RTSP reconnection avec exponential backoff
bool OpenCVCaptureManager::ShouldAttemptReconnect() {
    auto now = std::chrono::steady_clock::now();
    
    // Vérifier si on a atteint le max de tentatives
    int current_attempts = rtsp_reconnect_attempts_.load();
    if (current_attempts >= MAX_RECONNECT_ATTEMPTS) {
        std::cerr << "[OpenCVCaptureManager] Max RTSP reconnection attempts (" 
                  << MAX_RECONNECT_ATTEMPTS << ") reached. Giving up." << std::endl;
        return false;
    }
    
    // Calculer le délai avec exponential backoff: delay = base * 2^attempts
    // Tentative 0: 1s, 1: 2s, 2: 4s, 3: 8s, 4: 16s, 5: 32s...
    int delay_ms = BASE_RECONNECT_DELAY_MS * (1 << current_attempts);
    auto required_delay = std::chrono::milliseconds(delay_ms);
    
    // Vérifier si assez de temps s'est écoulé depuis la dernière tentative
    if (current_attempts > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_reconnect_attempt_);
        
        if (elapsed < required_delay) {
            // Pas encore le moment de réessayer
            return false;
        }
    }
    
    // Incrémenter le compteur de tentatives
    rtsp_reconnect_attempts_++;
    last_reconnect_attempt_ = now;
    
    std::cerr << "[OpenCVCaptureManager] RTSP reconnection attempt " 
              << rtsp_reconnect_attempts_.load() << "/" << MAX_RECONNECT_ATTEMPTS
              << " (delay: " << delay_ms << "ms)" << std::endl;
    
    return true;
}

void OpenCVCaptureManager::ResetReconnectAttempts() {
    if (rtsp_reconnect_attempts_.load() > 0) {
        std::cerr << "[OpenCVCaptureManager] RTSP reconnection successful! Resetting attempts counter." << std::endl;
    }
    rtsp_reconnect_attempts_ = 0;
}


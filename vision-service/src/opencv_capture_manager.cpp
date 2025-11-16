// src/opencv_capture_manager.cpp
// Implémentation du gestionnaire de capture avec OpenCV

#include "opencv_capture_manager.h"
#include <iostream>
#include <algorithm>
#include <chrono>

using namespace cv;

OpenCVCaptureManager::OpenCVCaptureManager(const std::string& camera_url)
    : CameraManager(camera_url), opencv_capture_(nullptr) {
    std::cerr << "[OpenCVCaptureManager] Constructed with URL: " << camera_url << std::endl;
}

OpenCVCaptureManager::~OpenCVCaptureManager() {
    try {
        Cleanup();
    } catch (const std::exception& e) {
        std::cerr << "[OpenCVCaptureManager] Exception in destructor: " << e.what() << std::endl;
    }
}

bool OpenCVCaptureManager::Initialize(const CameraConfig& config) {
    std::cerr << "[OpenCVCaptureManager] Initialize with OpenCV" << std::endl;
    
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
        SetMotionSensitivity(0.7);  // Sensibilité moyenne
        
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
    std::cerr << "[OpenCVCaptureManager] Setting up file capture: " << camera_url_ << std::endl;
    
    if (!opencv_capture_->open(camera_url_)) {
        SetError("Failed to open video file: " + camera_url_);
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
    std::cerr << "[OpenCVCaptureManager] Setting up RTSP capture: " << camera_url_ << std::endl;
    
    // Configuration spéciale pour RTSP
    opencv_capture_->set(CAP_PROP_BUFFERSIZE, 1); // Réduire la latence
    
    if (!opencv_capture_->open(camera_url_)) {
        SetError("Failed to open RTSP stream: " + camera_url_);
        return false;
    }

    // RTSP peut prendre du temps à se connecter
    Mat test_frame;
    int attempts = 0;
    const int max_attempts = 10;
    
    while (attempts < max_attempts) {
        if (opencv_capture_->read(test_frame) && !test_frame.empty()) {
            std::cerr << "[OpenCVCaptureManager] RTSP stream validated" << std::endl;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        attempts++;
    }

    SetError("RTSP stream timeout - no frames received");
    return false;
}

void OpenCVCaptureManager::OptimizeCapture() {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        return;
    }

    std::cerr << "[OpenCVCaptureManager] Optimizing capture settings..." << std::endl;

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

    std::cerr << "[OpenCVCaptureManager] Capture optimization completed" << std::endl;
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

    std::cerr << "[OpenCVCaptureManager] Capture validation successful: " 
              << test_frame.cols << "x" << test_frame.rows << std::endl;

    return true;
}

Frame OpenCVCaptureManager::CaptureFileFrame() {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        std::cerr << "[OpenCVCaptureManager] File capture not initialized" << std::endl;
        return CreateEmptyFrame();
    }

    Mat frame;
    if (!opencv_capture_->read(frame)) {
        // Fin de fichier ou erreur
        std::cerr << "[OpenCVCaptureManager] End of file or read error" << std::endl;
        
        // Option: boucler le fichier
        if (camera_type_ == CameraType::FILE_VIDEO) {
            opencv_capture_->set(CAP_PROP_POS_FRAMES, 0); // Revenir au début
            if (opencv_capture_->read(frame)) {
                std::cerr << "[OpenCVCaptureManager] File looped successfully" << std::endl;
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
        std::cerr << "[OpenCVCaptureManager] Webcam capture not initialized" << std::endl;
        return CreateEmptyFrame();
    }

    Mat frame;
    if (!opencv_capture_->read(frame)) {
        std::cerr << "[OpenCVCaptureManager] Webcam read failed" << std::endl;
        return CreateEmptyFrame();
    }

    if (frame.empty()) {
        std::cerr << "[OpenCVCaptureManager] Webcam returned empty frame" << std::endl;
        return CreateEmptyFrame();
    }

    // Appliquer détection de mouvement si activée
    ProcessMotionDetection(frame);

    return ConvertMatToFrame(frame);
}

Frame OpenCVCaptureManager::CaptureRtspFrame() {
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        std::cerr << "[OpenCVCaptureManager] RTSP capture not initialized" << std::endl;
        return CreateEmptyFrame();
    }

    Mat frame;
    if (!opencv_capture_->read(frame)) {
        std::cerr << "[OpenCVCaptureManager] RTSP read failed" << std::endl;
        
        // Tentative de reconnexion RTSP
        if (ShouldAttemptReconnect()) {
            std::cerr << "[OpenCVCaptureManager] Attempting RTSP reconnection..." << std::endl;
            opencv_capture_->release();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            if (opencv_capture_->open(camera_url_)) {
                std::cerr << "[OpenCVCaptureManager] RTSP reconnection successful" << std::endl;
                if (opencv_capture_->read(frame)) {
                    // Appliquer détection de mouvement si activée
                    if (!frame.empty()) {
                        ProcessMotionDetection(frame);
                    }
                    return ConvertMatToFrame(frame);
                }
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

Frame OpenCVCaptureManager::ConvertMatToFrame(const Mat& mat) const {
    if (mat.empty()) {
        return CreateEmptyFrame();
    }

    Frame frame(mat.cols, mat.rows, "bgr");
    
    // Copier les données
    size_t data_size = mat.total() * mat.elemSize();
    frame.data.resize(data_size);
    std::memcpy(frame.data.data(), mat.data, data_size);
    
    frame.timestamp = std::chrono::steady_clock::now();
    
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
        std::cerr << "[OpenCVCaptureManager] Frame data size mismatch" << std::endl;
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
    std::cerr << "[OpenCVCaptureManager] OpenCV capture thread started" << std::endl;
    LOG_INFO("OpenCV capture started with motion detection");
    
    return true;
}

bool OpenCVCaptureManager::StopCapture() {
    std::cerr << "[OpenCVCaptureManager] Stopping OpenCV capture" << std::endl;
    
    should_stop_capture_.store(true);
    is_capturing_.store(false);
    capture_active_ = false;
    
    // Attendre la fin du thread
    if (capture_thread_ && capture_thread_->joinable()) {
        capture_thread_->join();
        capture_thread_.reset();
        std::cerr << "[OpenCVCaptureManager] Capture thread stopped" << std::endl;
    }
    
    SetState(CameraState::READY);
    LOG_INFO("OpenCV capture stopped");
    
    return true;
}

void OpenCVCaptureManager::CaptureThreadLoop() {
    std::cerr << "[OpenCVCaptureManager] Capture thread loop started" << std::endl;
    
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
                    std::cerr << "[OpenCVCaptureManager] Unsupported camera type" << std::endl;
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
            
            // Log périodique (toutes les 5 secondes)
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
            if (elapsed.count() >= 5) {
                double fps = frame_count / static_cast<double>(elapsed.count());
                std::cerr << "[OpenCVCaptureManager] Captured " << frame_count 
                          << " frames in " << elapsed.count() << "s"
                          << " (FPS: " << fps << ")"
                          << " | Motion frames: " << motion_frames_count_.load() << std::endl;
                frame_count = 0;
                last_log_time = now;
            }
            
            // Notifier via callback si disponible
            if (frame_callback_) {
                NotifyFrameAvailable(frame);
            }
            
            // Contrôler le framerate (environ 30 FPS max)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            
        } catch (const std::exception& e) {
            std::cerr << "[OpenCVCaptureManager] Exception in capture loop: " 
                      << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::cerr << "[OpenCVCaptureManager] Capture thread loop ended" << std::endl;
}

// ============================================================================
// Détection de Mouvement avec BackgroundSubtractorMOG2
// ============================================================================

void OpenCVCaptureManager::EnableMotionDetection(bool enable) {
    motion_detection_enabled_.store(enable);
    if (enable && !background_subtractor_) {
        InitializeMotionDetector();
    }
    std::cerr << "[OpenCVCaptureManager] Motion detection " 
              << (enable ? "enabled" : "disabled") << std::endl;
}

bool OpenCVCaptureManager::IsMotionDetectionEnabled() const {
    return motion_detection_enabled_.load();
}

void OpenCVCaptureManager::SetMotionSensitivity(double sensitivity) {
    if (sensitivity >= 0.0 && sensitivity <= 1.0) {
        motion_sensitivity_.store(sensitivity);
        // Ajuster le seuil : plus sensible = seuil plus bas
        motion_detection_threshold_ = static_cast<int>(50 * (1.0 - sensitivity));
        std::cerr << "[OpenCVCaptureManager] Motion sensitivity set to " 
                  << sensitivity << " (threshold: " << motion_detection_threshold_ << ")" << std::endl;
    }
}

void OpenCVCaptureManager::InitializeMotionDetector() {
    std::cerr << "[OpenCVCaptureManager] Initializing MOG2 background subtractor" << std::endl;
    
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
    
    std::cerr << "[OpenCVCaptureManager] MOG2 initialized with history=" << history 
              << ", varThreshold=" << varThreshold << std::endl;
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
            motion_frames_count_++;
            std::cerr << "[OpenCVCaptureManager] Motion detected! Pixels: " 
                      << motion_pixels << " (threshold: " << motion_detection_threshold_ 
                      << "), frame #" << motion_frames_count_.load() << std::endl;
        }
        
        return motion_detected;
        
    } catch (const cv::Exception& e) {
        std::cerr << "[OpenCVCaptureManager] OpenCV exception in motion detection: " 
                  << e.what() << std::endl;
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

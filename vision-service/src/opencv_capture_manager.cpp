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
                return ConvertMatToFrame(frame);
            }
        }
        return CreateEmptyFrame();
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
                    return ConvertMatToFrame(frame);
                }
            }
        }
        return CreateEmptyFrame();
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
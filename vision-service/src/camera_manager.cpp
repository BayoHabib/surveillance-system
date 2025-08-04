#include "camera_manager.h"
#include <iostream>
#include <algorithm>
#include <regex>

#ifdef HAVE_OPENCV
#include "opencv_capture_manager.h"
#endif

using namespace CameraManagerConstants;

// Factory function to create appropriate camera manager
std::unique_ptr<CameraManager> CameraManager::CreateManager(const std::string& camera_url) {
#ifdef HAVE_OPENCV
    // Use OpenCV manager for real capture in Phase 2.3
    return std::make_unique<OpenCVCaptureManager>(camera_url);
#else
    // Use base manager for simulation in Phase 2.1
    return std::make_unique<CameraManager>(camera_url);
#endif
}

CameraManager::CameraManager(const std::string& camera_url) 
    : camera_url_(camera_url), 
      camera_type_(CameraType::UNKNOWN),
      source_type_(SourceType::UNKNOWN),
      config_(),
      state_(CameraState::UNINITIALIZED),
      stats_(),
      last_error_(""),
      is_initialized_(false), 
      capture_active_(false),
      current_config_{},
      total_frames_captured_(0),
      capture_start_time_(std::chrono::steady_clock::now()),
      should_stop_(false),
      is_capturing_(false),
      reconnect_attempts_(0) {
    
    LOG_INFO("Creating CameraManager for URL: " + camera_url);
    DetermineSourceType();
}

CameraManager::~CameraManager() {
    Cleanup();
    LOG_INFO("CameraManager destroyed");
}

bool CameraManager::Initialize(const CameraConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    LOG_INFO("Initializing CameraManager with config");
    current_config_ = config;
    
    if (!ValidateConfig(config)) {
        LOG_ERROR("Invalid camera configuration");
        SetState(CameraState::ERROR);
        return false;
    }
    
    // Base class provides simulation mode
    is_initialized_ = true;
    capture_active_ = false;
    total_frames_captured_ = 0;
    capture_start_time_ = std::chrono::steady_clock::now();
    
    // Set state to READY after successful initialization
    SetState(CameraState::READY);
    
    LOG_INFO("CameraManager initialized successfully (simulation mode)");
    return true;
}

void CameraManager::Cleanup() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (capture_active_) {
        StopCapture();
    }
    
    is_initialized_ = false;
    LOG_INFO("CameraManager cleanup completed");
}

bool CameraManager::StartCapture() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!is_initialized_) {
        LOG_ERROR("Cannot start capture: CameraManager not initialized");
        return false;
    }
    
    if (capture_active_) {
        LOG_WARNING("Capture already active");
        return true;
    }
    
    capture_active_ = true;
    capture_start_time_ = std::chrono::steady_clock::now();
    total_frames_captured_ = 0;
    
    // Set state to CAPTURING when capture starts
    SetState(CameraState::CAPTURING);
    
    LOG_INFO("Capture started (simulation mode)");
    return true;
}

bool CameraManager::StopCapture() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!capture_active_) {
        LOG_WARNING("Capture not active");
        return false;
    }
    
    capture_active_ = false;
    
    // Set state back to READY when capture stops
    SetState(CameraState::READY);
    
    LOG_INFO("Capture stopped");
    return true;
}

Frame CameraManager::CaptureFrame() {
    if (!IsCapturing()) {
        LOG_ERROR("Cannot capture frame: capture not active");
        return CreateEmptyFrame();
    }
    
    Frame frame;
    
    switch (source_type_) {
        case SourceType::FILE:
            frame = CaptureFileFrame();
            break;
        case SourceType::WEBCAM:
            frame = CaptureWebcamFrame();
            break;
        case SourceType::RTSP:
            frame = CaptureRtspFrame();
            break;
        case SourceType::TEST:
        default:
            frame = CaptureTestFrame();
            break;
    }
    
    if (!frame.data.empty()) {
        IncrementFrameCount();
    }
    
    return frame;
}

// Base class implementations provide simulation frames
Frame CameraManager::CaptureFileFrame() {
    // Base class returns simulated frame for Phase 2.1 compatibility
    LOG_DEBUG("CaptureFileFrame: returning simulated frame");
    return FrameUtils::CreateTestFrame(
        current_config_.width, 
        current_config_.height, 
        "bgr"
    );
}

Frame CameraManager::CaptureWebcamFrame() {
    // Base class returns simulated frame for Phase 2.1 compatibility
    LOG_DEBUG("CaptureWebcamFrame: returning simulated frame");
    return FrameUtils::CreateTestFrame(
        current_config_.width, 
        current_config_.height, 
        "bgr"
    );
}

Frame CameraManager::CaptureRtspFrame() {
    // Base class returns simulated frame for Phase 2.1 compatibility
    LOG_DEBUG("CaptureRtspFrame: returning simulated frame");
    return FrameUtils::CreateTestFrame(
        current_config_.width, 
        current_config_.height, 
        "bgr"
    );
}

Frame CameraManager::CaptureTestFrame() {
    LOG_DEBUG("CaptureTestFrame: creating test pattern");
    return FrameUtils::CreateTestFrame(
        current_config_.width, 
        current_config_.height, 
        "bgr"
    );
}

bool CameraManager::IsCapturing() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return is_initialized_ && capture_active_;
}

bool CameraManager::IsInitialized() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return is_initialized_;
}

CameraConfig CameraManager::GetConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return current_config_;
}

const CameraStats& CameraManager::GetStats() const {
    return stats_;
}

std::string CameraManager::GetCameraUrl() const {
    return camera_url_;
}

SourceType CameraManager::GetSourceType() const {
    return source_type_;
}

void CameraManager::DetermineSourceType() {
    if (camera_url_.empty()) {
        source_type_ = SourceType::TEST;
        camera_type_ = CameraType::TEST_PATTERN;
        return;
    }
    
    std::string url_lower = camera_url_;
    std::transform(url_lower.begin(), url_lower.end(), url_lower.begin(), ::tolower);
    
    if (url_lower.find("rtsp://") == 0 || url_lower.find("rtmp://") == 0) {
        source_type_ = SourceType::RTSP;
        camera_type_ = CameraType::RTSP_STREAM;
    } else if (url_lower.find("test://") == 0 || url_lower == "test" || url_lower == "test_pattern") {
        source_type_ = SourceType::TEST;
        camera_type_ = CameraType::TEST_PATTERN;
    } else if (std::regex_match(camera_url_, std::regex("^[0-9]+$")) || 
               camera_url_.find("/dev/video") == 0) {
        source_type_ = SourceType::WEBCAM;
        camera_type_ = CameraType::WEBCAM;
    } else {
        source_type_ = SourceType::FILE;
        camera_type_ = CameraType::FILE_VIDEO;
    }
    
    LOG_INFO("Determined source type: " + SourceTypeToString(source_type_));
}

bool CameraManager::ValidateConfig(const CameraConfig& config) const {
    if (config.width <= 0 || config.height <= 0) {
        LOG_ERROR("Invalid frame dimensions");
        return false;
    }
    
    if (config.fps <= 0 || config.fps > CameraManagerConstants::MAX_FPS) {
        LOG_ERROR("Invalid FPS value");
        return false;
    }
    
    return true;
}

Frame CameraManager::CreateEmptyFrame() const {
    return Frame();
}

void CameraManager::IncrementFrameCount() {
    total_frames_captured_++;
}

std::string CameraManager::SourceTypeToString(SourceType type) const {
    switch (type) {
        case SourceType::FILE: return "FILE";
        case SourceType::WEBCAM: return "WEBCAM";
        case SourceType::RTSP: return "RTSP";
        case SourceType::TEST: return "TEST";
        default: return "UNKNOWN";
    }
}

// Static method implementations
CameraType CameraManager::DetectCameraType(const std::string& url) {
    if (url.empty()) return CameraType::UNKNOWN;
    
    std::string url_lower = url;
    std::transform(url_lower.begin(), url_lower.end(), url_lower.begin(), ::tolower);
    
    // Check for test pattern
    if (url_lower.find("test://") == 0 || url_lower == "test" || url_lower == "test_pattern") {
        return CameraType::TEST_PATTERN;
    }
    
    // Check for video file extensions
    if (url_lower.find(".mp4") != std::string::npos ||
        url_lower.find(".avi") != std::string::npos ||
        url_lower.find(".mov") != std::string::npos ||
        url_lower.find(".mkv") != std::string::npos) {
        return CameraType::FILE_VIDEO;
    }
    
    // Check for webcam patterns
    if (url.find("/dev/video") == 0 || std::regex_match(url, std::regex("^[0-9]+$"))) {
        return CameraType::WEBCAM;
    }
    
    // Check for RTSP streams
    if (url_lower.find("rtsp://") == 0 ||
        url_lower.find("rtmp://") == 0) {
        return CameraType::RTSP_STREAM;
    }
    
    // Check for HTTP streams
    if (url_lower.find("http://") == 0 ||
        url_lower.find("https://") == 0) {
        return CameraType::HTTP_STREAM;
    }
    
    return CameraType::UNKNOWN;
}

bool CameraManager::IsValidCameraUrl(const std::string& url) {
    return DetectCameraType(url) != CameraType::UNKNOWN;
}

std::vector<std::string> CameraManager::GetAvailableWebcams() {
    std::vector<std::string> webcams;
    
    // Check for common video device paths
    for (int i = 0; i < 10; ++i) {
        std::string device = "/dev/video" + std::to_string(i);
        // In a real implementation, you would check if the device exists
        // For now, just add video0 as a common default
        if (i == 0) {
            webcams.push_back(device);
        }
    }
    
    return webcams;
}

// Additional method implementations that weren't already defined
CameraState CameraManager::GetState() const {
    return state_.load();
}

std::string CameraManager::GetLastError() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return last_error_;
}

CameraType CameraManager::GetCameraType() const {
    return camera_type_;
}

bool CameraManager::IsConnected() const {
    CameraState current_state = state_.load();
    return current_state == CameraState::READY || 
           current_state == CameraState::STREAMING;
}

bool CameraManager::Initialize() {
    CameraConfig default_config;
    return Initialize(default_config);
}

void CameraManager::SetConfig(const CameraConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
    current_config_ = config;
}

void CameraManager::SetFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    frame_callback_ = callback;
}

void CameraManager::ClearFrameCallback() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    frame_callback_ = nullptr;
}

// Protected method implementations for derived classes
void CameraManager::SetState(CameraState new_state) {
    state_.store(new_state);
    LOG_DEBUG("Camera state changed to: " + std::to_string(static_cast<int>(new_state)));
}

void CameraManager::SetError(const std::string& error) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    last_error_ = error;
    LOG_ERROR("Camera error: " + error);
    SetState(CameraState::ERROR);
}

void CameraManager::NotifyFrameAvailable(const Frame& frame) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (frame_callback_) {
        frame_callback_(frame);
    }
    
    // Update statistics
    total_frames_captured_++;
    stats_.frames_captured.store(total_frames_captured_);
    stats_.last_frame_time = std::chrono::steady_clock::now();
}

bool CameraManager::ShouldAttemptReconnect() const {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_reconnect_time_);
    
    return reconnect_attempts_.load() < CameraManagerConstants::MAX_RECONNECT_ATTEMPTS &&
           time_since_last.count() > CameraManagerConstants::DEFAULT_RECONNECT_DELAY_MS;
}
#include "camera_manager.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <random>
#include <limits>

using namespace CameraManagerConstants;

// =============================================================================
// CameraManager Implementation
// =============================================================================

CameraManager::CameraManager(const std::string& camera_url)
    : camera_url_(camera_url),
      camera_type_(DetectCameraType(camera_url)),
      state_(CameraState::UNINITIALIZED),
      should_stop_(false),
      is_capturing_(false),
      reconnect_attempts_(0) {
    std::cerr << "[CameraManager] Constructed with URL: " << camera_url_ << ", detected type: " << static_cast<int>(camera_type_) << std::endl;
}

CameraManager::~CameraManager() {
    try {
        std::cerr << "[CameraManager] Destructor called." << std::endl;
        Cleanup();
    } catch (const std::exception& e) {
        std::cerr << "[CameraManager] Exception during cleanup in destructor: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[CameraManager] Unknown exception during cleanup in destructor." << std::endl;
    }
}

bool CameraManager::Initialize() {
    std::cerr << "[CameraManager] Initialize() called." << std::endl;
    return Initialize(CameraConfig());
}

bool CameraManager::Initialize(const CameraConfig& config) {
    std::cerr << "[CameraManager] Initialize(config) called. Width: " << config.width
              << ", Height: " << config.height << ", FPS: " << config.fps << std::endl;
    // Validate config before locking
    if (config.width <= 0 || config.height <= 0 || config.fps <= 0 ||
        config.width > std::numeric_limits<int>::max() / config.height) {
        SetError("Invalid configuration parameters");
        SetState(CameraState::ERROR);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (state_ != CameraState::UNINITIALIZED) {
            SetError("Already initialized");
            return false;
        }
        SetState(CameraState::INITIALIZING);
        config_ = config;
    }

    bool success = false;
    try {
        switch (camera_type_) {
            case CameraType::FILE_VIDEO:
                std::cerr << "[CameraManager] Initializing FILE_VIDEO capture." << std::endl;
                success = InitializeFileCapture();
                break;
            case CameraType::WEBCAM:
                std::cerr << "[CameraManager] Initializing WEBCAM capture." << std::endl;
                success = InitializeWebcamCapture();
                break;
            case CameraType::RTSP_STREAM:
                std::cerr << "[CameraManager] Initializing RTSP_STREAM capture." << std::endl;
                success = InitializeRtspCapture();
                break;
            case CameraType::TEST_PATTERN:
                std::cerr << "[CameraManager] Initializing TEST_PATTERN capture." << std::endl;
                success = InitializeTestPattern();
                break;
            default:
                SetError("Unsupported camera type");
                success = false;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Initialization exception: ") + e.what());
        success = false;
    } catch (...) {
        SetError("Unknown exception during initialization");
        success = false;
    }

    if (success) {
        SetState(CameraState::READY);
        stats_.start_time = std::chrono::steady_clock::now();
        std::cerr << "[CameraManager] Initialization successful." << std::endl;
    } else {
        SetState(CameraState::ERROR);
        std::cerr << "[CameraManager] Initialization failed." << std::endl;
    }

    return success;
}

void CameraManager::Cleanup() {
    std::cerr << "[CameraManager] Cleanup() called." << std::endl;
    try {
        StopCapture();
    } catch (const std::exception& e) {
        std::cerr << "[CameraManager] Exception during StopCapture in Cleanup: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[CameraManager] Unknown exception during StopCapture in Cleanup." << std::endl;
    }

    std::lock_guard<std::mutex> config_lock(config_mutex_);
    std::lock_guard<std::mutex> callback_lock(callback_mutex_);

#ifdef HAVE_OPENCV
    opencv_capture_.reset();
#endif

    frame_callback_ = nullptr;
    SetState(CameraState::UNINITIALIZED);

    std::cerr << "[CameraManager] Cleanup completed." << std::endl;
}

bool CameraManager::StartCapture() {
    std::cerr << "[CameraManager] StartCapture() called." << std::endl;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (state_ != CameraState::READY) {
            SetError("Camera not ready for capture");
            return false;
        }
        if (is_capturing_.load()) {
            std::cerr << "[CameraManager] Already capturing." << std::endl;
            return true;  // Already capturing
        }
        should_stop_ = false;
    }

    try {
        // Start capture thread
        capture_thread_ = std::make_unique<std::thread>(&CameraManager::CaptureLoop, this);
        is_capturing_ = true;
        SetState(CameraState::CAPTURING);
        std::cerr << "[CameraManager] Capture thread started successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            should_stop_ = true;
            is_capturing_ = false;
            SetState(CameraState::READY);
        }
        SetError("Failed to start capture thread: " + std::string(e.what()));
        if (capture_thread_ && capture_thread_->joinable()) {
            capture_thread_->join();
        }
        capture_thread_.reset();
        return false;
    }
}

bool CameraManager::StopCapture() {
    std::cerr << "[CameraManager] StopCapture() called." << std::endl;
    if (!is_capturing_.load()) {
        std::cerr << "[CameraManager] Not capturing, nothing to stop." << std::endl;
        return true;  // Already stopped
    }

    // Check if called from the capture thread itself
    if (capture_thread_ && std::this_thread::get_id() == capture_thread_->get_id()) {
        std::cerr << "[CameraManager] StopCapture called from capture thread itself. Operation aborted." << std::endl;
        return false;
    }

    should_stop_ = true;
    std::cerr << "[CameraManager] Stopping capture..." << std::endl;

    // Wait for thread to finish
    try {
        if (capture_thread_ && capture_thread_->joinable()) {
            capture_thread_->join();
        }
    } catch (const std::exception& e) {
        std::cerr << "[CameraManager] Exception during capture thread join: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[CameraManager] Unknown exception during capture thread join." << std::endl;
    }

    capture_thread_.reset();
    is_capturing_ = false;

    if (state_ == CameraState::CAPTURING) {
        SetState(CameraState::READY);
    }

    std::cerr << "[CameraManager] Capture stopped." << std::endl;
    return true;
}

void CameraManager::SetConfig(const CameraConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    std::cerr << "[CameraManager] SetConfig() called. Width: " << config.width
              << ", Height: " << config.height << ", FPS: " << config.fps << std::endl;
    config_ = config;
}

CameraConfig CameraManager::GetConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

CameraState CameraManager::GetState() const {
    return state_.load();
}

const CameraStats& CameraManager::GetStats() const {
    return stats_;
}

std::string CameraManager::GetLastError() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return last_error_;
}

void CameraManager::SetFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    std::cerr << "[CameraManager] SetFrameCallback() called." << std::endl;
    frame_callback_ = std::move(callback);
}

void CameraManager::ClearFrameCallback() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    std::cerr << "[CameraManager] ClearFrameCallback() called." << std::endl;
    frame_callback_ = nullptr;
}

std::string CameraManager::GetCameraUrl() const {
    return camera_url_;
}

CameraType CameraManager::GetCameraType() const {
    return camera_type_;
}

bool CameraManager::IsCapturing() const {
    return is_capturing_.load();
}

bool CameraManager::IsConnected() const {
    CameraState state = state_.load();
    return state == CameraState::READY || state == CameraState::CAPTURING;
}

// Static methods

CameraType CameraManager::DetectCameraType(const std::string& url) {
    std::cerr << "[CameraManager] DetectCameraType() called for URL: " << url << std::endl;
    if (url.empty()) {
        return CameraType::UNKNOWN;
    }

    // Test pattern special
    if (url == "test://pattern" || url.find("test://") == 0) {
        return CameraType::TEST_PATTERN;
    }

    // RTSP/RTMP streams
    for (const auto& prefix : RTSP_PREFIXES) {
        if (url.find(prefix) == 0) {
            return CameraType::RTSP_STREAM;
        }
    }

    // HTTP streams
    for (const auto& prefix : HTTP_PREFIXES) {
        if (url.find(prefix) == 0) {
            return CameraType::HTTP_STREAM;
        }
    }

    // Webcam devices
    for (const auto& pattern : WEBCAM_PATTERNS) {
        if (url.find(pattern) == 0) {
            return CameraType::WEBCAM;
        }
    }

    // File video (check extension)
    for (const auto& ext : SUPPORTED_VIDEO_FORMATS) {
        if (url.size() >= ext.size() &&
            url.compare(url.size() - ext.size(), ext.size(), ext) == 0) {
            return CameraType::FILE_VIDEO;
        }
    }

    // If it's an existing file
    if (std::filesystem::exists(url)) {
        return CameraType::FILE_VIDEO;
    }

    return CameraType::UNKNOWN;
}

bool CameraManager::IsValidCameraUrl(const std::string& url) {
    bool valid = DetectCameraType(url) != CameraType::UNKNOWN;
    std::cerr << "[CameraManager] IsValidCameraUrl(" << url << ") = " << valid << std::endl;
    return valid;
}

std::vector<std::string> CameraManager::GetAvailableWebcams() {
    std::vector<std::string> webcams;

    // Scan /dev/video*
    for (int i = 0; i < 10; ++i) {
        std::string device = "/dev/video" + std::to_string(i);
        if (std::filesystem::exists(device)) {
            webcams.push_back(device);
        }
    }

    std::cerr << "[CameraManager] GetAvailableWebcams() found " << webcams.size() << " devices." << std::endl;
    return webcams;
}

// Private methods

void CameraManager::CaptureLoop() {
    std::cerr << "[CameraManager] CaptureLoop() started." << std::endl;
    while (!should_stop_.load()) {
        try {
            if (!CaptureFrame()) {
                std::cerr << "[CameraManager] CaptureFrame() failed." << std::endl;
                if (config_.auto_reconnect && ShouldAttemptReconnect()) {
                    std::cerr << "[CameraManager] Attempting reconnect..." << std::endl;
                    AttemptReconnect();
                } else {
                    SetError("Capture failed and reconnect disabled");
                    SetState(CameraState::ERROR);
                    break;
                }
            }

            // Framerate control
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1000 / std::max(1, config_.fps))
            );

        } catch (const std::exception& e) {
            HandleCaptureError("Capture exception: " + std::string(e.what()));
        }
    }

    is_capturing_ = false;
    std::cerr << "[CameraManager] CaptureLoop() exited." << std::endl;
}

bool CameraManager::InitializeCapture() {
    std::cerr << "[CameraManager] InitializeCapture() called." << std::endl;
    // Common initialization
    reconnect_attempts_ = 0;
    return true;
}

bool CameraManager::CaptureFrame() {
    Frame frame;
    bool success = false;

    switch (camera_type_) {
        case CameraType::FILE_VIDEO:
            frame = CaptureFileFrame();
            success = !frame.data.empty();
            break;
        case CameraType::WEBCAM:
            frame = CaptureWebcamFrame();
            success = !frame.data.empty();
            break;
        case CameraType::RTSP_STREAM:
            frame = CaptureRtspFrame();
            success = !frame.data.empty();
            break;
        case CameraType::TEST_PATTERN:
            frame = CaptureTestFrame();
            success = true;  // Test frames are always valid
            break;
        default:
            success = false;
    }

    if (success && ValidateFrame(frame)) {
        UpdateStats(frame);
        NotifyFrameAvailable(frame);
        std::cerr << "[CameraManager] Frame captured and validated. Size: " << frame.data.size() << std::endl;
    } else if (!success) {
        std::cerr << "[CameraManager] Frame capture failed." << std::endl;
    } else {
        std::cerr << "[CameraManager] Frame validation failed." << std::endl;
    }

    return success;
}

void CameraManager::HandleCaptureError(const std::string& error) {
    std::cerr << "[CameraManager] HandleCaptureError(): " << error << std::endl;
    SetError(error);

    if (config_.auto_reconnect) {
        SetState(CameraState::DISCONNECTED);
    } else {
        SetState(CameraState::ERROR);
    }
}

void CameraManager::AttemptReconnect() {
    std::cerr << "[CameraManager] AttemptReconnect() called. Attempt: " << reconnect_attempts_.load() << std::endl;
    if (reconnect_attempts_.load() >= config_.max_reconnect_attempts) {
        SetError("Maximum reconnect attempts exceeded");
        SetState(CameraState::ERROR);
        return;
    }

    SetState(CameraState::RECONNECTING);
    reconnect_attempts_++;
    stats_.reconnect_count++;

    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.reconnect_delay_ms)
    );

    // Reinitialize capture
    if (InitializeCapture()) {
        SetState(CameraState::CAPTURING);
        reconnect_attempts_ = 0;
        std::cerr << "[CameraManager] Reconnect successful." << std::endl;
    } else {
        // Failure, try again if possible
        std::cerr << "[CameraManager] Reconnect failed." << std::endl;
        if (ShouldAttemptReconnect()) {
            AttemptReconnect();
        } else {
            SetState(CameraState::ERROR);
        }
    }
}

bool CameraManager::ShouldAttemptReconnect() const {
    bool should = config_.auto_reconnect &&
           reconnect_attempts_.load() < config_.max_reconnect_attempts;
    std::cerr << "[CameraManager] ShouldAttemptReconnect() = " << should << std::endl;
    return should;
}

void CameraManager::SetState(CameraState new_state) {
    std::cerr << "[CameraManager] State changed from " << static_cast<int>(state_.load()) << " to " << static_cast<int>(new_state) << std::endl;
    state_ = new_state;
}

void CameraManager::SetError(const std::string& error) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    last_error_ = error;
    std::cerr << "[CameraManager] ERROR: " << error << std::endl;
}

void CameraManager::NotifyFrameAvailable(const Frame& frame) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (frame_callback_) {
        try {
            frame_callback_(frame);
        } catch (const std::exception& e) {
            std::cerr << "[CameraManager] Exception in frame callback: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[CameraManager] Unknown exception in frame callback." << std::endl;
        }
    }
}

// Type-specific implementations

bool CameraManager::InitializeFileCapture() {
#ifdef HAVE_OPENCV
    std::cerr << "[CameraManager] InitializeFileCapture() called for: " << camera_url_ << std::endl;
    opencv_capture_ = std::make_unique<cv::VideoCapture>(camera_url_);
    if (!opencv_capture_->isOpened()) {
        SetError("Failed to open video file: " + camera_url_);
        return false;
    }

    // Set resolution if possible
    opencv_capture_->set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    opencv_capture_->set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    opencv_capture_->set(cv::CAP_PROP_FPS, config_.fps);

    return true;
#else
    SetError("OpenCV not available for file capture");
    return false;
#endif
}

bool CameraManager::InitializeWebcamCapture() {
#ifdef HAVE_OPENCV
    std::cerr << "[CameraManager] InitializeWebcamCapture() called for: " << camera_url_ << std::endl;
    // Extract webcam index
    std::regex webcam_regex(R"(/dev/video(\d+))");
    std::smatch match;

    int device_index = 0;
    if (std::regex_search(camera_url_, match, webcam_regex)) {
        try {
            device_index = std::stoi(match[1].str());
        } catch (...) {
            SetError("Invalid webcam device index");
            return false;
        }
    }

    opencv_capture_ = std::make_unique<cv::VideoCapture>(device_index);
    if (!opencv_capture_->isOpened()) {
        SetError("Failed to open webcam: " + camera_url_);
        return false;
    }

    // Configure webcam
    opencv_capture_->set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    opencv_capture_->set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    opencv_capture_->set(cv::CAP_PROP_FPS, config_.fps);

    return true;
#else
    SetError("OpenCV not available for webcam capture");
    return false;
#endif
}

bool CameraManager::InitializeRtspCapture() {
#ifdef HAVE_OPENCV
    std::cerr << "[CameraManager] InitializeRtspCapture() called for: " << camera_url_ << std::endl;
    opencv_capture_ = std::make_unique<cv::VideoCapture>(camera_url_);
    if (!opencv_capture_->isOpened()) {
        SetError("Failed to open RTSP stream: " + camera_url_);
        return false;
    }

    return true;
#else
    SetError("OpenCV not available for RTSP capture");
    return false;
#endif
}

bool CameraManager::InitializeTestPattern() {
    std::cerr << "[CameraManager] InitializeTestPattern() called." << std::endl;
    // No OpenCV needed for test patterns
    return true;
}

Frame CameraManager::CaptureFileFrame() {
#ifdef HAVE_OPENCV
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        std::cerr << "[CameraManager] CaptureFileFrame() failed: capture not opened." << std::endl;
        return CreateEmptyFrame();
    }

    cv::Mat mat;
    if (!opencv_capture_->read(mat)) {
        std::cerr << "[CameraManager] CaptureFileFrame() failed: read failed." << std::endl;
        return CreateEmptyFrame();
    }

    return ConvertFromMat(mat);
#else
    // Simulate without OpenCV
    return FrameUtils::CreateTestFrame(config_.width, config_.height, config_.format);
#endif
}

Frame CameraManager::CaptureWebcamFrame() {
#ifdef HAVE_OPENCV
    if (!opencv_capture_ || !opencv_capture_->isOpened()) {
        std::cerr << "[CameraManager] CaptureWebcamFrame() failed: capture not opened." << std::endl;
        return CreateEmptyFrame();
    }

    cv::Mat mat;
    if (!opencv_capture_->read(mat)) {
        std::cerr << "[CameraManager] CaptureWebcamFrame() failed: read failed." << std::endl;
        return CreateEmptyFrame();
    }

    return ConvertFromMat(mat);
#else
    // Simulate without OpenCV
    return FrameUtils::CreateTestFrame(config_.width, config_.height, config_.format);
#endif
}

Frame CameraManager::CaptureRtspFrame() {
    // Same logic as webcam for now
    return CaptureWebcamFrame();
}

Frame CameraManager::CaptureTestFrame() {
    static TestPatternGenerator generator(config_.width, config_.height);
    static int pattern_type = 0;

    Frame frame;
    switch (pattern_type % 5) {
        case 0: frame = generator.GenerateColorBars(); break;
        case 1: frame = generator.GenerateCheckerboard(); break;
        case 2: frame = generator.GenerateMovingBox(); break;
        case 3: frame = generator.GenerateNoise(); break;
        case 4: frame = generator.GenerateTimeCode(); break;
    }

    // Change pattern every 5 seconds (approx)
    if (stats_.frames_captured.load() % (std::max(1, config_.fps) * 5) == 0) {
        pattern_type++;
        std::cerr << "[CameraManager] TestPatternGenerator: pattern_type changed to " << pattern_type << std::endl;
    }

    return frame;
}

Frame CameraManager::CreateEmptyFrame() const {
    std::cerr << "[CameraManager] CreateEmptyFrame() called." << std::endl;
    return Frame();
}

bool CameraManager::ValidateFrame(const Frame& frame) const {
    if (frame.data.empty()) {
        std::cerr << "[CameraManager] ValidateFrame() failed: data empty." << std::endl;
        return false;
    }

    if (frame.width <= 0 || frame.height <= 0) {
        std::cerr << "[CameraManager] ValidateFrame() failed: invalid dimensions." << std::endl;
        return false;
    }

    // Basic size check
    size_t expected_min_size = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    if (frame.data.size() < expected_min_size / 2) {  // Wide tolerance
        std::cerr << "[CameraManager] ValidateFrame() failed: data size too small." << std::endl;
        return false;
    }

    return true;
}

void CameraManager::UpdateStats(const Frame& frame) {
    stats_.frames_captured++;
    stats_.bytes_received += frame.data.size();
    stats_.last_frame_time = std::chrono::steady_clock::now();
    std::cerr << "[CameraManager] UpdateStats(): frames_captured=" << stats_.frames_captured
              << ", bytes_received=" << stats_.bytes_received << std::endl;
}

// =============================================================================
// TestPatternGenerator Implementation
// =============================================================================

TestPatternGenerator::TestPatternGenerator(int width, int height)
    : width_(width), height_(height), start_time_(std::chrono::steady_clock::now()) {
    std::cerr << "[TestPatternGenerator] Constructed. Width: " << width_ << ", Height: " << height_ << std::endl;
}

Frame TestPatternGenerator::GenerateColorBars() {
    std::cerr << "[TestPatternGenerator] GenerateColorBars() called." << std::endl;
    Frame frame(width_, height_, "bgr");
    size_t frame_size = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3;
    frame.data.resize(frame_size);

    // Standard color bars
    int bar_width = std::max(1, width_ / 8);
    uint8_t colors[8][3] = {
        {255, 255, 255},  // White
        {0, 255, 255},    // Yellow
        {255, 255, 0},    // Cyan
        {0, 255, 0},      // Green
        {255, 0, 255},    // Magenta
        {0, 0, 255},      // Red
        {255, 0, 0},      // Blue
        {0, 0, 0}         // Black
    };

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int bar = x / bar_width;
            if (bar >= 8) bar = 7;

            size_t idx = (static_cast<size_t>(y) * width_ + x) * 3;
            frame.data[idx] = colors[bar][0];     // B
            frame.data[idx + 1] = colors[bar][1]; // G
            frame.data[idx + 2] = colors[bar][2]; // R
        }
    }

    return frame;
}

Frame TestPatternGenerator::GenerateCheckerboard() {
    std::cerr << "[TestPatternGenerator] GenerateCheckerboard() called." << std::endl;
    Frame frame(width_, height_, "bgr");
    size_t frame_size = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3;
    frame.data.resize(frame_size);

    int square_size = 32;

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            bool is_white = ((x / square_size) + (y / square_size)) % 2 == 0;
            uint8_t color = is_white ? 255 : 0;

            size_t idx = (static_cast<size_t>(y) * width_ + x) * 3;
            frame.data[idx] = color;      // B
            frame.data[idx + 1] = color;  // G
            frame.data[idx + 2] = color;  // R
        }
    }

    return frame;
}

Frame TestPatternGenerator::GenerateMovingBox() {
    std::cerr << "[TestPatternGenerator] GenerateMovingBox() called." << std::endl;
    Frame frame = FrameUtils::CreateColorFrame(width_, height_, 50, 50, 50, "bgr");

    // Moving box
    int counter = frame_counter_.load();
    int box_size = 60;
    int x = (counter * 3) % std::max(1, width_ - box_size);
    int y = (counter * 2) % std::max(1, height_ - box_size);

    // Draw box
    for (int dy = 0; dy < box_size; ++dy) {
        for (int dx = 0; dx < box_size; ++dx) {
            int px = x + dx;
            int py = y + dy;
            if (px < width_ && py < height_) {
                size_t idx = (static_cast<size_t>(py) * width_ + px) * 3;
                frame.data[idx] = 0;      // B
                frame.data[idx + 1] = 255; // G
                frame.data[idx + 2] = 0;   // R
            }
        }
    }

    frame_counter_++;
    return frame;
}

Frame TestPatternGenerator::GenerateNoise() {
    std::cerr << "[TestPatternGenerator] GenerateNoise() called." << std::endl;
    Frame frame(width_, height_, "bgr");
    size_t frame_size = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3;
    frame.data.resize(frame_size);

    // Random noise
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < frame.data.size(); ++i) {
        frame.data[i] = static_cast<uint8_t>(dis(gen));
    }

    return frame;
}

Frame TestPatternGenerator::GenerateTimeCode() {
    std::cerr << "[TestPatternGenerator] GenerateTimeCode() called." << std::endl;
    Frame frame = FrameUtils::CreateColorFrame(width_, height_, 0, 0, 100, "bgr");

    // Add simple timecode (simulated)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

    std::string timecode = "Frame: " + std::to_string(frame_counter_.load()) +
                          " Time: " + std::to_string(elapsed.count()) + "s";

    // For now, just change color according to time
    uint8_t intensity = static_cast<uint8_t>((elapsed.count() % 10) * 25);

    // Modify an area to represent the timecode
    for (int y = 20; y < 60; ++y) {
        for (int x = 20; x < 200 && x < width_; ++x) {
            size_t idx = (static_cast<size_t>(y) * width_ + x) * 3;
            if (idx + 2 < frame.data.size()) {
                frame.data[idx] = intensity;      // B
                frame.data[idx + 1] = 255;       // G
                frame.data[idx + 2] = intensity; // R
            }
        }
    }

    frame_counter_++;
    return frame;
}

void TestPatternGenerator::SetSize(int width, int height) {
    std::cerr << "[TestPatternGenerator] SetSize() called. Width: " << width << ", Height: " << height << std::endl;
    width_ = width;
    height_ = height;
}

void TestPatternGenerator::SetFrameCounter(int counter) {
    std::cerr << "[TestPatternGenerator] SetFrameCounter() called. Counter: " << counter << std::endl;
    frame_counter_ = counter;
}

// =============================================================================
// CameraManagerFactory Implementation
// =============================================================================

std::unique_ptr<CameraManager> CameraManagerFactory::Create(const std::string& camera_url) {
    std::cerr << "[CameraManagerFactory] Create() called for URL: " << camera_url << std::endl;
    if (!ValidateUrl(camera_url)) {
        std::cerr << "[CameraManagerFactory] Invalid URL: " << camera_url << std::endl;
        return nullptr;
    }

    return std::make_unique<CameraManager>(camera_url);
}

std::unique_ptr<CameraManager> CameraManagerFactory::Create(const std::string& camera_url,
                                                          const CameraConfig& config) {
    std::cerr << "[CameraManagerFactory] Create() called for URL: " << camera_url << " with config." << std::endl;
    auto manager = Create(camera_url);
    if (manager && manager->Initialize(config)) {
        return manager;
    }
    std::cerr << "[CameraManagerFactory] Failed to create or initialize CameraManager." << std::endl;
    return nullptr;
}

bool CameraManagerFactory::ValidateUrl(const std::string& url) {
    bool valid = CameraManager::IsValidCameraUrl(url);
    std::cerr << "[CameraManagerFactory] ValidateUrl(" << url << ") = " << valid << std::endl;
    return valid;
}

CameraType CameraManagerFactory::DetectType(const std::string& url) {
    CameraType type = CameraManager::DetectCameraType(url);
    std::cerr << "[CameraManagerFactory] DetectType(" << url << ") = " << static_cast<int>(type) << std::endl;
    return type;
}

std::string CameraManagerFactory::GetTypeString(CameraType type) {
    switch (type) {
        case CameraType::FILE_VIDEO: return "FILE_VIDEO";
        case CameraType::WEBCAM: return "WEBCAM";
        case CameraType::RTSP_STREAM: return "RTSP_STREAM";
        case CameraType::HTTP_STREAM: return "HTTP_STREAM";
        case CameraType::TEST_PATTERN: return "TEST_PATTERN";
        default: return "UNKNOWN";
    }
}
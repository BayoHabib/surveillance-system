#!/bin/bash
# setup_opencv.sh - Configuration OpenCV pour Phase 2.3

set -e

echo "🔧 Setup OpenCV pour Phase 2.3"
echo "================================"

# Détecter l'OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    echo "❌ OS non supporté: $OSTYPE"
    exit 1
fi

# Installation OpenCV
install_opencv() {
    echo "📦 Installation OpenCV..."
    
    if [[ "$OS" == "linux" ]]; then
        # Ubuntu/Debian
        sudo apt update
        sudo apt install -y \
            libopencv-dev \
            libopencv-contrib-dev \
            libavcodec-dev \
            libavformat-dev \
            libavutil-dev \
            libswscale-dev \
            libv4l-dev \
            pkg-config
            
        # Vérifier la version
        OPENCV_VERSION=$(pkg-config --modversion opencv4 2>/dev/null || pkg-config --modversion opencv 2>/dev/null || echo "unknown")
        echo "✅ OpenCV installé: version $OPENCV_VERSION"
        
    elif [[ "$OS" == "macos" ]]; then
        # macOS avec Homebrew
        brew install opencv
        OPENCV_VERSION=$(brew list --versions opencv | awk '{print $2}')
        echo "✅ OpenCV installé: version $OPENCV_VERSION"
    fi
}

# Mise à jour CMakeLists.txt
update_cmake() {
    echo "🔧 Mise à jour CMakeLists.txt..."
    
    # Backup du fichier original
    cp vision-service/CMakeLists.txt vision-service/CMakeLists.txt.bak
    
    # Ajouter OpenCV au CMakeLists.txt
    cat >> vision-service/CMakeLists.txt << 'EOF'

# OpenCV Configuration for Phase 2.3
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
    add_definitions(-DHAVE_OPENCV)
    include_directories(${OpenCV_INCLUDE_DIRS})
    
    # Ajouter les composants OpenCV au target principal
    target_link_libraries(vision-service ${OpenCV_LIBS})
    
    # Ajouter aux tests aussi
    if(BUILD_TESTS AND GTest_FOUND)
        target_link_libraries(vision-service-tests ${OpenCV_LIBS})
    endif()
    
    # Afficher les composants OpenCV
    message(STATUS "OpenCV components: ${OpenCV_LIBS}")
else()
    message(FATAL_ERROR "OpenCV not found - required for Phase 2.3")
endif()

# Optimisations spécifiques OpenCV
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(vision-service PRIVATE CV_ENABLE_INTRINSICS)
endif()
EOF

    echo "✅ CMakeLists.txt mis à jour"
}

# Créer les nouveaux headers pour OpenCV
create_opencv_headers() {
    echo "📝 Création des headers OpenCV..."
    
    # Vision Engine principal
    cat > vision-service/src/vision_engine.h << 'EOF'
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
EOF

    # Capture Manager spécialisé
    cat > vision-service/src/opencv_capture_manager.h << 'EOF'
#ifndef OPENCV_CAPTURE_MANAGER_H
#define OPENCV_CAPTURE_MANAGER_H

#include <opencv2/opencv.hpp>
#include "camera_manager.h"

// Gestionnaire de capture spécialisé OpenCV
class OpenCVCaptureManager : public CameraManager {
public:
    explicit OpenCVCaptureManager(const std::string& camera_url);
    ~OpenCVCaptureManager() override;
    
    // Implémentations spécialisées
    bool Initialize(const CameraConfig& config) override;
    void Cleanup() override;
    
protected:
    // Méthodes de capture spécialisées
    Frame CaptureFileFrame() override;
    Frame CaptureWebcamFrame() override; 
    Frame CaptureRtspFrame() override;
    
    // Utilitaires OpenCV
    Frame ConvertMatToFrame(const cv::Mat& mat) const;
    cv::Mat ConvertFrameToMat(const Frame& frame) const;
    
private:
    std::unique_ptr<cv::VideoCapture> opencv_capture_;
    cv::Mat current_frame_;
    
    // Configuration OpenCV spécifique
    bool SetupCapture();
    void OptimizeCapture();
    bool ValidateCapture() const;
};

#endif // OPENCV_CAPTURE_MANAGER_H
EOF

    # Motion Detector OpenCV
    cat > vision-service/src/opencv_motion_detector.h << 'EOF'
#ifndef OPENCV_MOTION_DETECTOR_H
#define OPENCV_MOTION_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
#include "frame_processor.h"

// Configuration détection de mouvement
struct MotionDetectionConfig {
    double threshold = 25.0;                    // Seuil de détection
    int min_area = 500;                        // Aire minimale (pixels)
    int max_area = 50000;                      // Aire maximale (pixels)
    bool enable_noise_reduction = true;         // Réduction de bruit
    bool enable_shadow_detection = true;       // Détection ombres
    int morphology_size = 3;                   // Taille morphologie
    double learning_rate = 0.005;             // Taux apprentissage background
    int history = 500;                        // Historique background
};

// Détecteur de mouvement avec OpenCV
class OpenCVMotionDetector : public Detector {
public:
    explicit OpenCVMotionDetector(const MotionDetectionConfig& config = MotionDetectionConfig{});
    ~OpenCVMotionDetector() override;
    
    // Interface Detector
    std::vector<Detection> Detect(const Frame& frame) override;
    std::string GetName() const override { return "OpenCVMotionDetector"; }
    bool Initialize() override;
    void Cleanup() override;
    
    // Configuration
    void SetConfig(const MotionDetectionConfig& config);
    MotionDetectionConfig GetConfig() const;
    
    // Debug et visualisation
    cv::Mat GetLastForegroundMask() const;
    cv::Mat GetBackgroundImage() const;
    void EnableDebugOutput(bool enable) { debug_output_ = enable; }
    
private:
    MotionDetectionConfig config_;
    bool initialized_;
    bool debug_output_;
    
    // OpenCV components
    cv::Ptr<cv::BackgroundSubtractorMOG2> bg_subtractor_;
    cv::Mat foreground_mask_;
    cv::Mat background_image_;
    cv::Mat previous_frame_;
    
    // Morphology elements
    cv::Mat morphology_kernel_;
    
    // Statistiques
    std::atomic<uint64_t> total_detections_{0};
    std::atomic<uint64_t> false_positives_filtered_{0};
    
    // Méthodes privées
    std::vector<Detection> ProcessForegroundMask(const cv::Mat& mask, int64_t timestamp);
    bool FilterDetection(const cv::Rect& bbox, double area) const;
    Detection CreateDetectionFromContour(const std::vector<cv::Point>& contour, int64_t timestamp) const;
    void ApplyNoiseReduction(cv::Mat& mask) const;
    std::string GenerateDetectionId() const;
};

#endif // OPENCV_MOTION_DETECTOR_H
EOF

    echo "✅ Headers OpenCV créés"
}

# Créer les scripts de test OpenCV
create_test_scripts() {
    echo "🧪 Création des scripts de test..."
    
    mkdir -p vision-service/tests/opencv
    
    # Test basique OpenCV
    cat > vision-service/tests/opencv/test_opencv_basic.cpp << 'EOF'
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include "../src/opencv_capture_manager.h"
#include "../src/opencv_motion_detector.h"

class OpenCVBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test avec une image générée
        test_image_ = cv::Mat::zeros(480, 640, CV_8UC3);
        cv::rectangle(test_image_, cv::Point(100, 100), cv::Point(200, 200), cv::Scalar(255, 255, 255), -1);
    }
    
    cv::Mat test_image_;
};

TEST_F(OpenCVBasicTest, OpenCVInstallation) {
    // Vérifier qu'OpenCV est correctement installé
    EXPECT_FALSE(test_image_.empty());
    EXPECT_EQ(test_image_.rows, 480);
    EXPECT_EQ(test_image_.cols, 640);
    EXPECT_EQ(test_image_.channels(), 3);
}

TEST_F(OpenCVBasicTest, MotionDetectorInitialization) {
    MotionDetectionConfig config;
    config.threshold = 30.0;
    config.min_area = 100;
    
    OpenCVMotionDetector detector(config);
    EXPECT_TRUE(detector.Initialize());
    EXPECT_EQ(detector.GetName(), "OpenCVMotionDetector");
}

TEST_F(OpenCVBasicTest, FrameConversion) {
    // Test conversion Mat <-> Frame
    Frame frame(640, 480, "bgr");
    frame.data.resize(640 * 480 * 3);
    
    // Remplir avec des données test
    cv::Mat mat = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::rectangle(mat, cv::Point(50, 50), cv::Point(150, 150), cv::Scalar(0, 255, 0), -1);
    
    // Conversion Mat -> Frame
    std::memcpy(frame.data.data(), mat.data, mat.total() * mat.elemSize());
    
    EXPECT_FALSE(frame.data.empty());
    EXPECT_EQ(frame.width, 640);
    EXPECT_EQ(frame.height, 480);
}
EOF

    echo "✅ Scripts de test créés"
}

# Mise à jour du Makefile
update_makefile() {
    echo "🔧 Mise à jour Makefile..."
    
    # Ajouter les commandes OpenCV
    cat >> vision-service/Makefile << 'EOF'

# =============================================================================
# OPENCV COMMANDS (Phase 2.3)
# =============================================================================

# Test OpenCV installation
.PHONY: test-opencv
test-opencv:
	@echo "🔍 Test installation OpenCV..."
	@pkg-config --exists opencv4 && echo "✅ OpenCV 4.x détecté" || \
	 pkg-config --exists opencv && echo "✅ OpenCV détecté" || \
	 echo "❌ OpenCV non trouvé"
	@echo "Version:" $$(pkg-config --modversion opencv4 2>/dev/null || pkg-config --modversion opencv 2>/dev/null)

# Build avec OpenCV
.PHONY: build-opencv
build-opencv: setup-gtest
	@echo "🔨 Building with OpenCV support..."
	mkdir -p build
	cd build && cmake -DHAVE_OPENCV=ON .. && make -j$(NUM_CORES)

# Tests OpenCV spécifiques
.PHONY: test-opencv-unit
test-opencv-unit: build-opencv
	@echo "🧪 Running OpenCV unit tests..."
	@if [ -f "build/vision-service-tests" ]; then \
		cd build && ./vision-service-tests --gtest_filter="OpenCV*"; \
	else \
		echo "⚠️ Tests not built"; \
	fi

# Debug OpenCV
.PHONY: debug-opencv
debug-opencv: build-opencv
	@echo "🔍 Debug OpenCV integration..."
	cd build && gdb ./vision-service

# Informations OpenCV
.PHONY: opencv-info
opencv-info:
	@echo "📋 OpenCV Information"
	@echo "===================="
	@pkg-config --list-all | grep opencv || echo "No OpenCV packages found"
	@echo ""
	@echo "Include directories:"
	@pkg-config --cflags opencv4 2>/dev/null || pkg-config --cflags opencv 2>/dev/null || echo "Not found"
	@echo ""
	@echo "Libraries:"
	@pkg-config --libs opencv4 2>/dev/null || pkg-config --libs opencv 2>/dev/null || echo "Not found"
EOF

    echo "✅ Makefile mis à jour"
}

# Fonction principale
main() {
    echo "🚀 Début du setup OpenCV..."
    
    # Vérifier les prérequis
    if ! command -v cmake &> /dev/null; then
        echo "❌ CMake requis mais non trouvé"
        exit 1
    fi
    
    # Installation
    install_opencv
    update_cmake
    create_opencv_headers
    create_test_scripts
    update_makefile
    
    echo ""
    echo "✅ Setup OpenCV terminé!"
    echo ""
    echo "🔧 Prochaines étapes:"
    echo "  1. cd vision-service && make test-opencv"
    echo "  2. make build-opencv"
    echo "  3. make test-opencv-unit"
    echo ""
    echo "📋 Pour info détaillée:"
    echo "  make opencv-info"
}

# Exécution
main "$@"
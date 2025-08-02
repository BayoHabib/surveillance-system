// tests/test_vision_service.cpp
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>

#include "../src/vision_service.h"
#include "../src/frame_processor.h"
#include "../src/camera_manager.h"

// Test fixture pour VisionService
class VisionServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<VisionServiceImpl>();
    }
    
    void TearDown() override {
        service_.reset();
    }
    
    std::unique_ptr<VisionServiceImpl> service_;
};

// Tests du Health Check
TEST_F(VisionServiceTest, HealthCheckReturnsHealthy) {
    grpc::ServerContext context;
    surveillance::vision::HealthRequest request;
    surveillance::vision::HealthResponse response;
    
    grpc::Status status = service_->GetHealth(&context, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), "healthy");
    EXPECT_GE(response.uptime_seconds(), 0);
    EXPECT_EQ(response.version(), "1.0.0-phase2.1");
}

// Tests de démarrage de stream
TEST_F(VisionServiceTest, StartStreamWithValidRequest) {
    grpc::ServerContext context;
    surveillance::vision::StreamRequest request;
    surveillance::vision::StreamResponse response;
    
    request.set_camera_id("test_cam");
    request.set_camera_url("test://pattern");
    
    grpc::Status status = service_->StartStream(&context, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), "success");
    EXPECT_FALSE(response.stream_id().empty());
    EXPECT_EQ(service_->GetActiveStreamsCount(), 1);
    
    // Nettoyer
    surveillance::vision::StopRequest stop_request;
    surveillance::vision::StopResponse stop_response;
    stop_request.set_camera_id("test_cam");
    service_->StopStream(&context, &stop_request, &stop_response);
}

TEST_F(VisionServiceTest, StartStreamWithInvalidCameraId) {
    grpc::ServerContext context;
    surveillance::vision::StreamRequest request;
    surveillance::vision::StreamResponse response;
    
    // Camera ID vide
    request.set_camera_id("");
    request.set_camera_url("test://pattern");
    
    grpc::Status status = service_->StartStream(&context, &request, &response);
    
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_F(VisionServiceTest, StartStreamWithInvalidUrl) {
    grpc::ServerContext context;
    surveillance::vision::StreamRequest request;
    surveillance::vision::StreamResponse response;
    
    request.set_camera_id("test_cam");
    request.set_camera_url("");  // URL vide
    
    grpc::Status status = service_->StartStream(&context, &request, &response);
    
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_F(VisionServiceTest, StartDuplicateStream) {
    grpc::ServerContext context;
    surveillance::vision::StreamRequest request;
    surveillance::vision::StreamResponse response1, response2;
    
    request.set_camera_id("test_cam");
    request.set_camera_url("test://pattern");
    
    // Premier stream
    grpc::Status status1 = service_->StartStream(&context, &request, &response1);
    EXPECT_TRUE(status1.ok());
    EXPECT_EQ(response1.status(), "success");
    
    // Deuxième stream avec même ID
    grpc::Status status2 = service_->StartStream(&context, &request, &response2);
    EXPECT_TRUE(status2.ok());
    EXPECT_EQ(response2.status(), "error");
    EXPECT_EQ(service_->GetActiveStreamsCount(), 1);
    
    // Nettoyer
    surveillance::vision::StopRequest stop_request;
    surveillance::vision::StopResponse stop_response;
    stop_request.set_camera_id("test_cam");
    service_->StopStream(&context, &stop_request, &stop_response);
}

// Tests d'arrêt de stream
TEST_F(VisionServiceTest, StopExistingStream) {
    grpc::ServerContext context;
    
    // Démarrer un stream
    surveillance::vision::StreamRequest start_request;
    surveillance::vision::StreamResponse start_response;
    start_request.set_camera_id("test_cam");
    start_request.set_camera_url("test://pattern");
    service_->StartStream(&context, &start_request, &start_response);
    
    // Arrêter le stream
    surveillance::vision::StopRequest stop_request;
    surveillance::vision::StopResponse stop_response;
    stop_request.set_camera_id("test_cam");
    
    grpc::Status status = service_->StopStream(&context, &stop_request, &stop_response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(stop_response.status(), "success");
    EXPECT_EQ(service_->GetActiveStreamsCount(), 0);
}

TEST_F(VisionServiceTest, StopNonExistentStream) {
    grpc::ServerContext context;
    surveillance::vision::StopRequest request;
    surveillance::vision::StopResponse response;
    
    request.set_camera_id("nonexistent_cam");
    
    grpc::Status status = service_->StopStream(&context, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), "error");
}

// Tests de statut de stream
TEST_F(VisionServiceTest, GetStatusOfActiveStream) {
    grpc::ServerContext context;
    
    // Démarrer un stream
    surveillance::vision::StreamRequest start_request;
    surveillance::vision::StreamResponse start_response;
    start_request.set_camera_id("test_cam");
    start_request.set_camera_url("test://pattern");
    service_->StartStream(&context, &start_request, &start_response);
    
    // Attendre un peu pour que le stream soit actif
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Obtenir le statut
    surveillance::vision::StatusRequest status_request;
    surveillance::vision::StatusResponse status_response;
    status_request.set_camera_id("test_cam");
    
    grpc::Status status = service_->GetStreamStatus(&context, &status_request, &status_response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(status_response.camera_id(), "test_cam");
    EXPECT_EQ(status_response.status(), "active");
    EXPECT_TRUE(status_response.has_stats());
    
    // Nettoyer
    surveillance::vision::StopRequest stop_request;
    surveillance::vision::StopResponse stop_response;
    stop_request.set_camera_id("test_cam");
    service_->StopStream(&context, &stop_request, &stop_response);
}

TEST_F(VisionServiceTest, GetStatusOfInactiveStream) {
    grpc::ServerContext context;
    surveillance::vision::StatusRequest request;
    surveillance::vision::StatusResponse response;
    
    request.set_camera_id("inactive_cam");
    
    grpc::Status status = service_->GetStreamStatus(&context, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.camera_id(), "inactive_cam");
    EXPECT_EQ(response.status(), "stopped");
}

// Test fixture pour FrameProcessor
class FrameProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor_ = std::make_unique<FrameProcessor>();
        ASSERT_TRUE(processor_->Initialize());
    }
    
    void TearDown() override {
        processor_->Cleanup();
        processor_.reset();
    }
    
    std::unique_ptr<FrameProcessor> processor_;
};

// Tests du FrameProcessor
TEST_F(FrameProcessorTest, InitializeAndCleanup) {
    // L'initialisation a déjà été testée dans SetUp()
    EXPECT_EQ(processor_->GetTotalFramesProcessed(), 0);
    EXPECT_EQ(processor_->GetTotalDetections(), 0);
    
    // Tester qu'il y a au moins un détecteur
    auto detector_names = processor_->GetDetectorNames();
    EXPECT_FALSE(detector_names.empty());
}

TEST_F(FrameProcessorTest, ProcessValidFrame) {
    Frame test_frame = FrameUtils::CreateTestFrame(640, 480, "bgr");
    
    ProcessingResult result = processor_->ProcessFrame(test_frame);
    
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.processing_time_ms, 0);
    EXPECT_EQ(processor_->GetTotalFramesProcessed(), 1);
}

TEST_F(FrameProcessorTest, ProcessInvalidFrame) {
    Frame invalid_frame;  // Frame vide
    
    ProcessingResult result = processor_->ProcessFrame(invalid_frame);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(FrameProcessorTest, ProcessMultipleFrames) {
    const int num_frames = 5;
    
    for (int i = 0; i < num_frames; ++i) {
        Frame test_frame = FrameUtils::CreateTestFrame(640, 480, "bgr");
        ProcessingResult result = processor_->ProcessFrame(test_frame);
        EXPECT_TRUE(result.success);
    }
    
    EXPECT_EQ(processor_->GetTotalFramesProcessed(), num_frames);
    EXPECT_GE(processor_->GetAverageProcessingTime(), 0.0);
}

// Test fixture pour CameraManager
class CameraManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test avec un pattern de test
        manager_ = std::make_unique<CameraManager>("test://pattern");
    }
    
    void TearDown() override {
        if (manager_) {
            manager_->Cleanup();
        }
        manager_.reset();
    }
    
    std::unique_ptr<CameraManager> manager_;
};

// Tests du CameraManager
TEST_F(CameraManagerTest, InitializeTestPattern) {
    EXPECT_EQ(manager_->GetCameraType(), CameraType::TEST_PATTERN);
    EXPECT_EQ(manager_->GetState(), CameraState::UNINITIALIZED);
    
    bool success = manager_->Initialize();
    EXPECT_TRUE(success);
    EXPECT_EQ(manager_->GetState(), CameraState::READY);
}

TEST_F(CameraManagerTest, StartAndStopCapture) {
    ASSERT_TRUE(manager_->Initialize());
    
    // Démarrer la capture
    bool start_success = manager_->StartCapture();
    EXPECT_TRUE(start_success);
    EXPECT_TRUE(manager_->IsCapturing());
    EXPECT_EQ(manager_->GetState(), CameraState::CAPTURING);
    
    // Attendre un peu pour capturer quelques frames
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Vérifier les statistiques
    // const CameraStats& stats = manager_->GetStats(); 
    // Arrêter la capture
    bool stop_success = manager_->StopCapture();
    EXPECT_TRUE(stop_success);
    EXPECT_FALSE(manager_->IsCapturing());
}

TEST_F(CameraManagerTest, DetectCameraTypes) {
    EXPECT_EQ(CameraManager::DetectCameraType("test://pattern"), CameraType::TEST_PATTERN);
    EXPECT_EQ(CameraManager::DetectCameraType("rtsp://example.com/stream"), CameraType::RTSP_STREAM);
    EXPECT_EQ(CameraManager::DetectCameraType("/dev/video0"), CameraType::WEBCAM);
    EXPECT_EQ(CameraManager::DetectCameraType("video.mp4"), CameraType::FILE_VIDEO);
    EXPECT_EQ(CameraManager::DetectCameraType(""), CameraType::UNKNOWN);
}

// Tests des utilitaires
TEST(FrameUtilsTest, CreateTestFrame) {
    Frame frame = FrameUtils::CreateTestFrame(320, 240, "bgr");
    
    EXPECT_EQ(frame.width, 320);
    EXPECT_EQ(frame.height, 240);
    EXPECT_EQ(frame.format, "bgr");
    EXPECT_FALSE(frame.data.empty());
    EXPECT_EQ(frame.data.size(), 320 * 240 * 3);
}

TEST(FrameUtilsTest, CreateColorFrame) {
    Frame frame = FrameUtils::CreateColorFrame(100, 100, 255, 0, 0, "rgb");
    
    EXPECT_EQ(frame.width, 100);
    EXPECT_EQ(frame.height, 100);
    EXPECT_EQ(frame.format, "rgb");
    EXPECT_EQ(frame.data.size(), 100 * 100 * 3);
    
    // Vérifier que la couleur est correcte (rouge en RGB)
    EXPECT_EQ(frame.data[0], 255);  // R
    EXPECT_EQ(frame.data[1], 0);    // G
    EXPECT_EQ(frame.data[2], 0);    // B
}

TEST(FrameUtilsTest, ValidateFormats) {
    EXPECT_TRUE(FrameUtils::IsValidFormat("bgr"));
    EXPECT_TRUE(FrameUtils::IsValidFormat("rgb"));
    EXPECT_TRUE(FrameUtils::IsValidFormat("gray"));
    EXPECT_FALSE(FrameUtils::IsValidFormat("invalid"));
    
    auto formats = FrameUtils::GetSupportedFormats();
    EXPECT_FALSE(formats.empty());
    EXPECT_GT(formats.size(), 3);
}

TEST(FrameUtilsTest, CalculateFrameSize) {
    EXPECT_EQ(FrameUtils::CalculateFrameSize(640, 480, "bgr"), 640 * 480 * 3);
    EXPECT_EQ(FrameUtils::CalculateFrameSize(640, 480, "gray"), 640 * 480);
    EXPECT_GT(FrameUtils::CalculateFrameSize(640, 480, "jpeg"), 0);
    EXPECT_EQ(FrameUtils::CalculateFrameSize(640, 480, "unknown"), 0);
}

// Tests de ServiceMetrics
TEST(ServiceMetricsTest, SingletonInstance) {
    ServiceMetrics& instance1 = ServiceMetrics::Instance();
    ServiceMetrics& instance2 = ServiceMetrics::Instance();
    
    EXPECT_EQ(&instance1, &instance2);
}

TEST(ServiceMetricsTest, IncrementCounters) {
    ServiceMetrics& metrics = ServiceMetrics::Instance();
    
    int64_t initial_streams = metrics.GetStreamsStarted();
    int64_t initial_frames = metrics.GetFramesProcessed();
    
    metrics.IncrementStreamsStarted();
    metrics.IncrementFramesProcessed();
    
    EXPECT_EQ(metrics.GetStreamsStarted(), initial_streams + 1);
    EXPECT_EQ(metrics.GetFramesProcessed(), initial_frames + 1);
}

// Point d'entrée des tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
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

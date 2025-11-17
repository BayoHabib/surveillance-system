// create_test_video.cpp - Create a test video file for real video streaming demonstration
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

using namespace cv;

int main() {
    const std::string output_file = "test_camera_feed.mp4";
    const int width = 640;
    const int height = 480;
    const int fps = 15;
    const int duration_seconds = 30; // 30 second video
    const int total_frames = fps * duration_seconds;
    
    // Create video writer
    VideoWriter writer(output_file, VideoWriter::fourcc('M','P','4','V'), fps, Size(width, height));
    
    if (!writer.isOpened()) {
        std::cerr << "Error: Could not open video writer for " << output_file << std::endl;
        return -1;
    }
    
    std::cout << "Creating test video: " << output_file << std::endl;
    std::cout << "Resolution: " << width << "x" << height << std::endl;
    std::cout << "FPS: " << fps << std::endl;
    std::cout << "Duration: " << duration_seconds << " seconds" << std::endl;
    
    for (int frame_num = 0; frame_num < total_frames; ++frame_num) {
        Mat frame(height, width, CV_8UC3);
        
        // Create dynamic background (moving gradient)
        float time = static_cast<float>(frame_num) / fps;
        int offset = static_cast<int>(time * 50) % width;
        
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int intensity = ((x + offset) * 255) / width;
                frame.at<Vec3b>(y, x) = Vec3b(intensity / 3, intensity / 2, intensity);
            }
        }
        
        // Add moving circle (simulates motion)
        float circle_x = width/2 + 100 * sin(time * 2.0);
        float circle_y = height/2 + 50 * cos(time * 3.0);
        circle(frame, Point(static_cast<int>(circle_x), static_cast<int>(circle_y)), 
               30, Scalar(0, 255, 255), -1);
        
        // Add timestamp
        std::string timestamp = "Frame: " + std::to_string(frame_num) + 
                               " Time: " + std::to_string(time).substr(0, 4) + "s";
        putText(frame, timestamp, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, 
                Scalar(255, 255, 255), 2);
        
        // Add "LIVE" indicator
        putText(frame, "LIVE CAMERA FEED", Point(10, height - 20), 
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 0), 2);
        
        // Add motion detection simulation
        if (frame_num % 60 == 0) { // Every 4 seconds
            rectangle(frame, Point(200, 200), Point(400, 350), 
                     Scalar(0, 0, 255), 3);
            putText(frame, "MOTION DETECTED", Point(210, 190), 
                    FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 0, 255), 2);
        }
        
        writer.write(frame);
        
        if (frame_num % 30 == 0) {
            std::cout << "Progress: " << (frame_num * 100 / total_frames) << "%" << std::endl;
        }
    }
    
    writer.release();
    std::cout << "Video created successfully: " << output_file << std::endl;
    
    return 0;
}

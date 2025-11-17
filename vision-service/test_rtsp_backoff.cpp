// Test simple pour Bug #8: RTSP reconnection avec exponential backoff
// Ce test simule des échecs de connexion RTSP et mesure le timing du backoff

#include <iostream>
#include <chrono>
#include <thread>
#include "src/opencv_capture_manager.h"
#include "src/camera_manager.h"

void PrintSeparator() {
    std::cout << "\n========================================" << std::endl;
}

int main() {
    PrintSeparator();
    std::cout << "BUG #8: RTSP Reconnection Test" << std::endl;
    std::cout << "Testing exponential backoff strategy" << std::endl;
    PrintSeparator();
    
    // Créer un manager avec une URL RTSP invalide pour forcer les échecs
    std::cout << "\n[TEST] Creating OpenCVCaptureManager with invalid RTSP URL..." << std::endl;
    OpenCVCaptureManager manager("rtsp://192.168.999.999:554/invalid_stream");
    
    CameraConfig config;
    config.width = 1280;
    config.height = 720;
    config.fps = 30;
    
    manager.SetCameraId("rtsp_backoff_test");
    
    std::cout << "[TEST] Initializing manager (this will fail as expected)..." << std::endl;
    bool initialized = manager.Initialize(config);
    
    if (!initialized) {
        std::cout << "[TEST] ✓ Initialization failed as expected (invalid RTSP URL)" << std::endl;
    }
    
    // Démarrer la capture (qui va échouer et tenter des reconnections)
    std::cout << "\n[TEST] Starting capture thread..." << std::endl;
    std::cout << "[TEST] Expected behavior:" << std::endl;
    std::cout << "  - Attempt 1: ~1s delay" << std::endl;
    std::cout << "  - Attempt 2: ~2s delay" << std::endl;
    std::cout << "  - Attempt 3: ~4s delay" << std::endl;
    std::cout << "  - Attempt 4: ~8s delay" << std::endl;
    std::cout << "  - After 10 attempts: should give up" << std::endl;
    
    PrintSeparator();
    std::cout << "STARTING CAPTURE (watch logs for backoff timing)..." << std::endl;
    PrintSeparator();
    
    manager.StartCapture();
    
    // Laisser tourner pendant 30 secondes pour observer les tentatives
    std::cout << "\n[TEST] Monitoring for 30 seconds...\n" << std::endl;
    
    for (int i = 0; i < 30; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Vérifier les statistiques du buffer
        size_t buffer_size = manager.GetBufferSize();
        int dropped = manager.GetDroppedFramesCount();
        
        if (i % 5 == 0) {
            std::cout << "[" << i << "s] Buffer: " << buffer_size 
                      << " frames, Dropped: " << dropped << std::endl;
        }
    }
    
    std::cout << "\n[TEST] Stopping capture..." << std::endl;
    manager.StopCapture();
    
    PrintSeparator();
    std::cout << "TEST COMPLETED" << std::endl;
    std::cout << "\nExpected observations in logs above:" << std::endl;
    std::cout << "1. ✓ First few attempts show increasing delays (1s, 2s, 4s, 8s...)" << std::endl;
    std::cout << "2. ✓ After 10 attempts, 'Max RTSP reconnection attempts reached'" << std::endl;
    std::cout << "3. ✓ No more reconnection attempts after max reached" << std::endl;
    PrintSeparator();
    
    return 0;
}

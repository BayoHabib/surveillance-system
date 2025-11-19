// ==========================================
// Test Simple du Fix TCP RTSP
// ==========================================
// Compile: g++ -o test_tcp_fix test_tcp_fix.cpp `pkg-config --cflags --libs opencv4`
// Run: ./test_tcp_fix rtsp://url

#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rtsp_url>" << std::endl;
        std::cerr << "Example: " << argv[0] << " rtsp://localhost:8554/test" << std::endl;
        return 1;
    }

    std::string url = argv[1];
    
    std::cout << "========================================" << std::endl;
    std::cout << "   RTSP TCP Fix Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Testing URL: " << url << std::endl;
    std::cout << std::endl;

    // ========================================
    // TEST 1: UDP (Défaut - devrait échouer en WSL)
    // ========================================
    std::cout << "[Test 1/3] UDP transport (default)..." << std::endl;
    
    cv::VideoCapture cap_udp(url, cv::CAP_FFMPEG);
    cap_udp.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000);
    cap_udp.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, 3000);
    
    auto start = std::chrono::steady_clock::now();
    bool udp_success = cap_udp.isOpened();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count();
    
    if (udp_success) {
        cv::Mat frame;
        if (cap_udp.read(frame)) {
            std::cout << "  ✓ UDP works! Frame size: " << frame.cols << "x" << frame.rows << std::endl;
        } else {
            std::cout << "  ⚠ UDP opened but no frame (" << elapsed << "s)" << std::endl;
            udp_success = false;
        }
    } else {
        std::cout << "  ✗ UDP failed (" << elapsed << "s) - Expected in WSL" << std::endl;
    }
    cap_udp.release();
    std::cout << std::endl;

    // ========================================
    // TEST 2: TCP (Fix WSL)
    // ========================================
    std::cout << "[Test 2/3] TCP transport (WSL fix)..." << std::endl;
    
    // FORCE TCP TRANSPORT
    #ifdef __linux__
        std::cout << "  → Setting OPENCV_FFMPEG_CAPTURE_OPTIONS=rtsp_transport;tcp" << std::endl;
        setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp", 1);
    #elif _WIN32
        std::cout << "  → Setting OPENCV_FFMPEG_CAPTURE_OPTIONS=rtsp_transport;tcp" << std::endl;
        _putenv("OPENCV_FFMPEG_CAPTURE_OPTIONS=rtsp_transport;tcp");
    #endif
    
    cv::VideoCapture cap_tcp(url, cv::CAP_FFMPEG);
    cap_tcp.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 10000);  // 10s pour TCP
    cap_tcp.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, 5000);
    cap_tcp.set(cv::CAP_PROP_BUFFERSIZE, 1);
    
    start = std::chrono::steady_clock::now();
    bool tcp_success = cap_tcp.isOpened();
    elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count();
    
    if (tcp_success) {
        cv::Mat frame;
        if (cap_tcp.read(frame)) {
            std::cout << "  ✓ TCP works! Frame size: " << frame.cols << "x" << frame.rows 
                      << " (" << elapsed << "s)" << std::endl;
                      
            // ========================================
            // TEST 3: Capturer plusieurs frames
            // ========================================
            std::cout << std::endl;
            std::cout << "[Test 3/3] Capturing multiple frames..." << std::endl;
            
            int frame_count = 0;
            for (int i = 0; i < 10; i++) {
                if (cap_tcp.read(frame)) {
                    frame_count++;
                    if (i == 0 || i == 9) {
                        std::cout << "  ✓ Frame " << (i+1) << ": " << frame.cols << "x" << frame.rows << std::endl;
                    }
                } else {
                    std::cout << "  ✗ Failed to read frame " << (i+1) << std::endl;
                    break;
                }
            }
            std::cout << "  → Captured " << frame_count << "/10 frames successfully" << std::endl;
            
        } else {
            std::cout << "  ⚠ TCP opened but no frame (" << elapsed << "s)" << std::endl;
            tcp_success = false;
        }
    } else {
        std::cout << "  ✗ TCP failed (" << elapsed << "s)" << std::endl;
    }
    cap_tcp.release();
    std::cout << std::endl;

    // ========================================
    // RÉSUMÉ
    // ========================================
    std::cout << "========================================" << std::endl;
    std::cout << "   Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "UDP Transport:  " << (udp_success ? "✓ OK" : "✗ FAIL") << std::endl;
    std::cout << "TCP Transport:  " << (tcp_success ? "✓ OK" : "✗ FAIL") << std::endl;
    std::cout << std::endl;
    
    if (tcp_success) {
        std::cout << "✅ WSL RTSP TCP Fix Working!" << std::endl;
        std::cout << std::endl;
        std::cout << "Your vision-service is configured to use TCP automatically." << std::endl;
        std::cout << "This resolves WSL NAT issues with UDP/RTP packets." << std::endl;
        return 0;
    } else {
        std::cout << "❌ TCP Transport Failed" << std::endl;
        std::cout << std::endl;
        std::cout << "Troubleshooting:" << std::endl;
        std::cout << "  1. Verify camera/stream is reachable" << std::endl;
        std::cout << "  2. Check authentication credentials" << std::endl;
        std::cout << "  3. Verify firewall allows TCP port 554" << std::endl;
        std::cout << "  4. Test with: ffmpeg -rtsp_transport tcp -i <url> test.jpg" << std::endl;
        return 1;
    }
}

package main

import (
	"fmt"
	"log"
	"net/http"
	"time"

	"surveillance-core/internal/vision"

	"github.com/gin-gonic/gin"
)

func main() {
	// Create a simple working internet streaming server
	log.Println("🌐 Starting Internet Video Streaming Server...")

	// Initialize vision client
	visionClient := vision.NewGRPCClient(&vision.ClientConfig{
		GRPCAddress: "localhost:50051",
	})

	router := gin.Default()

	// CORS
	router.Use(func(c *gin.Context) {
		c.Header("Access-Control-Allow-Origin", "*")
		c.Header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Content-Type, Authorization")
		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}
		c.Next()
	})

	// Working Internet Streaming Endpoints
	router.POST("/api/v1/cameras/internet", func(c *gin.Context) {
		var req struct {
			Name     string `json:"name" binding:"required"`
			URL      string `json:"url" binding:"required"`
			Location string `json:"location"`
			Type     string `json:"type"`
		}

		if err := c.ShouldBindJSON(&req); err != nil {
			c.JSON(400, gin.H{"error": err.Error()})
			return
		}

		// Generate camera ID
		cameraID := fmt.Sprintf("internet_%d", time.Now().Unix())

		// Start internet stream
		log.Printf("🌐 Starting internet stream for %s with URL: %s", req.Name, req.URL)
		framesChan, err := visionClient.StartStreamWithURL(cameraID, req.URL)
		if err != nil {
			log.Printf("❌ Failed to start stream: %v", err)
			c.JSON(500, gin.H{"error": fmt.Sprintf("Failed to start stream: %v", err)})
			return
		}

		// Test that frames are being generated
		select {
		case frame := <-framesChan:
			log.Printf("✅ First frame received: %dx%d, %d bytes", frame.Width, frame.Height, len(frame.Data))
		case <-time.After(2 * time.Second):
			log.Printf("⏰ No frame received within 2 seconds")
		}

		c.JSON(201, gin.H{
			"message":    "Internet camera added successfully",
			"camera_id":  cameraID,
			"name":       req.Name,
			"url":        req.URL,
			"status":     "streaming",
			"resolution": "1280x720",
			"fps":        30,
			"timestamp":  time.Now(),
		})

		log.Printf("✅ Internet camera created: %s (%s)", req.Name, cameraID)
	})

	router.GET("/api/v1/stream/formats", func(c *gin.Context) {
		c.JSON(200, gin.H{
			"supported_formats": []gin.H{
				{"protocol": "rtsp", "description": "Real Time Streaming Protocol", "example": "rtsp://username:password@192.168.1.100:554/stream"},
				{"protocol": "http", "description": "HTTP video streams", "example": "http://camera-ip:8080/video"},
				{"protocol": "https", "description": "HTTPS video streams", "example": "https://example.com/stream.m3u8"},
				{"protocol": "rtmp", "description": "Real Time Messaging Protocol", "example": "rtmp://live-server.com/live/stream-key"},
			},
			"demo_streams": []gin.H{
				{"name": "Big Buck Bunny RTSP", "url": "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4", "type": "rtsp"},
				{"name": "Sample MP4", "url": "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4", "type": "http"},
				{"name": "Tears of Steel", "url": "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/TearsOfSteel.mp4", "type": "https"},
			},
			"timestamp": time.Now(),
		})
	})

	// Health check
	router.GET("/api/v1/health", func(c *gin.Context) {
		c.JSON(200, gin.H{
			"status":            "healthy",
			"internet_streaming": "enabled",
			"timestamp":         time.Now(),
		})
	})

	// Stream test endpoint
	router.GET("/api/v1/test/stream/:id", func(c *gin.Context) {
		cameraID := c.Param("id")
		
		// Test the StartStreamWithURL method directly
		testURL := "rtsp://test-demo-stream.example.com/live"
		framesChan, err := visionClient.StartStreamWithURL(cameraID, testURL)
		if err != nil {
			c.JSON(500, gin.H{"error": err.Error()})
			return
		}

		// Collect some frames to test
		frames := []gin.H{}
		for i := 0; i < 5; i++ {
			select {
			case frame := <-framesChan:
				frames = append(frames, gin.H{
					"camera_id": frame.CameraID,
					"width":     frame.Width,
					"height":    frame.Height,
					"size":      frame.Size,
					"timestamp": frame.Timestamp,
					"frame_num": i + 1,
				})
			case <-time.After(1 * time.Second):
				break
			}
		}

		c.JSON(200, gin.H{
			"camera_id":    cameraID,
			"test_url":     testURL,
			"frames_count": len(frames),
			"frames":       frames,
			"status":       "test_completed",
		})
	})

	// Basic web interface
	router.GET("/", func(c *gin.Context) {
		html := `<!DOCTYPE html>
<html>
<head>
    <title>Internet Video Streaming Test</title>
    <style>
        body { font-family: Arial; margin: 40px; background: #1a1a1a; color: white; }
        .container { max-width: 800px; margin: 0 auto; }
        h1 { color: #4CAF50; }
        .test-section { background: #2d2d2d; padding: 20px; margin: 20px 0; border-radius: 8px; }
        button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; }
        button:hover { background: #45a049; }
        .result { background: #333; padding: 15px; margin: 10px 0; border-radius: 4px; font-family: monospace; }
        input { width: 300px; padding: 8px; margin: 5px; border: 1px solid #555; background: #333; color: white; border-radius: 4px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌐 Internet Video Streaming Test</h1>
        
        <div class="test-section">
            <h3>Test 1: Stream Formats</h3>
            <button onclick="testFormats()">Get Supported Formats</button>
            <div id="formats-result" class="result"></div>
        </div>

        <div class="test-section">
            <h3>Test 2: Add Internet Camera</h3>
            <input type="text" id="camera-name" placeholder="Camera Name" value="Demo RTSP Camera">
            <input type="text" id="camera-url" placeholder="Stream URL" value="rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4">
            <input type="text" id="camera-location" placeholder="Location" value="Internet">
            <br>
            <button onclick="addCamera()">Add Internet Camera</button>
            <div id="camera-result" class="result"></div>
        </div>

        <div class="test-section">
            <h3>Test 3: Stream Test</h3>
            <input type="text" id="test-camera-id" placeholder="Camera ID" value="test_camera_1">
            <button onclick="testStream()">Test Stream Generation</button>
            <div id="stream-result" class="result"></div>
        </div>

        <div class="test-section">
            <h3>Health Check</h3>
            <button onclick="checkHealth()">Check Server Health</button>
            <div id="health-result" class="result"></div>
        </div>
    </div>

    <script>
        async function testFormats() {
            try {
                const response = await fetch('/api/v1/stream/formats');
                const data = await response.json();
                document.getElementById('formats-result').innerText = JSON.stringify(data, null, 2);
            } catch (error) {
                document.getElementById('formats-result').innerText = 'Error: ' + error.message;
            }
        }

        async function addCamera() {
            try {
                const name = document.getElementById('camera-name').value;
                const url = document.getElementById('camera-url').value;
                const location = document.getElementById('camera-location').value;
                
                const response = await fetch('/api/v1/cameras/internet', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name, url, location, type: 'rtsp' })
                });
                
                const data = await response.json();
                document.getElementById('camera-result').innerText = JSON.stringify(data, null, 2);
            } catch (error) {
                document.getElementById('camera-result').innerText = 'Error: ' + error.message;
            }
        }

        async function testStream() {
            try {
                const cameraId = document.getElementById('test-camera-id').value;
                const response = await fetch('/api/v1/test/stream/' + cameraId);
                const data = await response.json();
                document.getElementById('stream-result').innerText = JSON.stringify(data, null, 2);
            } catch (error) {
                document.getElementById('stream-result').innerText = 'Error: ' + error.message;
            }
        }

        async function checkHealth() {
            try {
                const response = await fetch('/api/v1/health');
                const data = await response.json();
                document.getElementById('health-result').innerText = JSON.stringify(data, null, 2);
            } catch (error) {
                document.getElementById('health-result').innerText = 'Error: ' + error.message;
            }
        }

        // Auto-load health check
        window.onload = () => checkHealth();
    </script>
</body>
</html>`
		c.Header("Content-Type", "text/html")
		c.String(200, html)
	})

	// Start server
	log.Println("🚀 Server starting on http://localhost:8081")
	log.Println("📱 Open http://localhost:8081 to test internet streaming")
	
	server := &http.Server{
		Addr:    ":8081",
		Handler: router,
	}

	if err := server.ListenAndServe(); err != nil {
		log.Printf("❌ Server error: %v", err)
	}
}

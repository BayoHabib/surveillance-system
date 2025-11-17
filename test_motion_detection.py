#!/usr/bin/env python3
"""Test de détection de mouvement avec OpenCV MOG2"""

import grpc
import sys
import time
from threading import Thread

sys.path.insert(0, '/workspaces/surveillance-system/internal/vision/proto')
import vision_pb2
import vision_pb2_grpc

def test_motion_detection():
    """Test la détection de mouvement en temps réel"""
    
    channel = grpc.insecure_channel('localhost:50051')
    stub = vision_pb2_grpc.VisionServiceStub(channel)
    
    print("🎬 Testing Motion Detection with OpenCV MOG2\n")
    print("="*60)
    
    # 1. Health check
    health_req = vision_pb2.HealthRequest()
    health_resp = stub.GetHealth(health_req)
    print(f"✅ Health: {health_resp.status}\n")
    
    # 2. Start stream avec vidéo de test
    stream_req = vision_pb2.StreamRequest(
        camera_id="motion_test_cam",
        camera_url="/workspaces/surveillance-system/vision-service/test_video.mp4"
    )
    
    stream_resp = stub.StartStream(stream_req)
    print(f"📹 Stream: {stream_resp.status}")
    print(f"   Message: {stream_resp.message}")
    
    if stream_resp.status != "success":
        print("❌ Failed to start stream")
        return
    
    print(f"   Stream ID: {stream_resp.stream_id}\n")
    
    print("🔄 Simulating frame capture for motion detection...")
    print("   (Vision service will process frames and detect motion)\n")
    
    # Simuler la capture de frames pendant 15 secondes
    # Le service C++ capture et analyse automatiquement
    for i in range(15):
        time.sleep(1)
        
        # Vérifier le statut périodiquement
        if i % 3 == 0:
            status_req = vision_pb2.StatusRequest(camera_id="motion_test_cam")
            status_resp = stub.GetStreamStatus(status_req)
            
            print(f"  [{i}s] Status: {status_resp.status}", end="")
            if status_resp.HasField("stats"):
                print(f" | Frames: {status_resp.stats.frames_processed}", end="")
                print(f" | Detections: {status_resp.stats.detections_count}")
            else:
                print()
    
    print("\n📊 Final Status:")
    status_req = vision_pb2.StatusRequest(camera_id="motion_test_cam")
    status_resp = stub.GetStreamStatus(status_req)
    
    print(f"   Status: {status_resp.status}")
    if status_resp.HasField("stats"):
        stats = status_resp.stats
        print(f"   Frames processed: {stats.frames_processed}")
        print(f"   Detections: {stats.detections_count}")
        print(f"   FPS: {stats.fps_actual:.2f}")
        print(f"   Uptime: {stats.uptime_seconds}s")
    
    # 3. Stop stream
    stop_req = vision_pb2.StopRequest(camera_id="motion_test_cam")
    stop_resp = stub.StopStream(stop_req)
    print(f"\n🛑 Stop: {stop_resp.status}")
    
    print("\n" + "="*60)
    print("✅ Motion detection test completed!")
    print("\nNote: Check vision-service logs for detailed motion detection output")

if __name__ == "__main__":
    test_motion_detection()

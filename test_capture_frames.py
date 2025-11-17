#!/usr/bin/env python3
"""Test direct de capture de frames via gRPC"""

import grpc
import sys
import time

# Add path to proto generated files
sys.path.insert(0, '/workspaces/surveillance-system/internal/vision/proto')

import vision_pb2
import vision_pb2_grpc

def test_frame_capture():
    """Test la capture de frames avec OpenCV"""
    
    # Connect to vision service
    channel = grpc.insecure_channel('localhost:50051')
    stub = vision_pb2_grpc.VisionServiceStub(channel)
    
    print("🧪 Testing Frame Capture with OpenCV\n")
    print("="*50)
    
    # 1. Health check
    health_req = vision_pb2.HealthRequest()
    health_resp = stub.GetHealth(health_req)
    print(f"✅ Health: {health_resp.status}")
    print(f"   Message: {health_resp.message}\n")
    
    # 2. Start stream
    stream_req = vision_pb2.StreamRequest(
        camera_id="frame_test_001",
        camera_url="/workspaces/surveillance-system/vision-service/test_video.mp4"
    )
    
    stream_resp = stub.StartStream(stream_req)
    print(f"📹 Stream Status: {stream_resp.status}")
    print(f"   Message: {stream_resp.message}")
    
    if stream_resp.status != "success":
        print("❌ Failed to start stream")
        return
    
    print(f"   Stream ID: {stream_resp.stream_id}\n")
    
    # 3. Try to process frames (bidirectional streaming)
    print("🎬 Requesting frame processing...\n")
    
    def frame_requests():
        """Generator for frame requests"""
        for i in range(10):  # Request 10 frames
            yield vision_pb2.FrameRequest(
                camera_id="frame_test_001",
                timestamp=int(time.time() * 1000),
                sequence_number=i
            )
            time.sleep(0.1)  # 100ms between requests
    
    try:
        responses = stub.ProcessFrames(frame_requests())
        frame_count = 0
        for response in responses:
            frame_count += 1
            stats = response.processing_stats
            print(f"  Frame {frame_count}:")
            print(f"    Processing time: {stats.processing_time_ms}ms")
            print(f"    Detections: {stats.detections_count}")
            print(f"    CPU: {stats.cpu_usage}%")
            print(f"    Memory: {stats.memory_usage_mb}MB")
            
        print(f"\n✅ Processed {frame_count} frames successfully")
        
    except grpc.RpcError as e:
        print(f"❌ RPC Error: {e.code()} - {e.details()}")
    
    # 4. Get status
    print("\n📊 Final Status:")
    status_req = vision_pb2.StatusRequest(camera_id="frame_test_001")
    status_resp = stub.GetStreamStatus(status_req)
    
    print(f"   Status: {status_resp.status}")
    print(f"   Message: {status_resp.message}")
    if status_resp.HasField("stats"):
        print(f"   Frames processed: {status_resp.stats.frames_processed}")
        print(f"   Detections: {status_resp.stats.detections_count}")
        print(f"   FPS: {status_resp.stats.fps_actual:.2f}")
    
    # 5. Stop stream
    stop_req = vision_pb2.StopRequest(camera_id="frame_test_001")
    stop_resp = stub.StopStream(stop_req)
    print(f"\n🛑 Stop Status: {stop_resp.status}")
    
    print("\n" + "="*50)
    print("✅ Test completed!")

if __name__ == "__main__":
    test_frame_capture()

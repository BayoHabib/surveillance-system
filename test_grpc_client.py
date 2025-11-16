#!/usr/bin/env python3
"""Test client gRPC pour le vision service"""

import sys
import grpc
sys.path.insert(0, '/workspaces/surveillance-system/internal/vision/proto')

import vision_pb2
import vision_pb2_grpc

def test_health():
    """Test du health check"""
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = vision_pb2_grpc.VisionServiceStub(channel)
        request = vision_pb2.HealthRequest()
        try:
            response = stub.GetHealth(request)
            print(f"✅ Health Check: {response.status}")
            print(f"   Message: {response.message}")
            print(f"   Active streams: {response.active_streams}")
            return True
        except Exception as e:
            print(f"❌ Health check failed: {e}")
            return False

def test_start_stream():
    """Test du démarrage de stream avec la vidéo de test"""
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = vision_pb2_grpc.VisionServiceStub(channel)
        
        # Préparer la configuration
        config = vision_pb2.StreamConfig(
            width=640,
            height=480,
            fps=30,
            format="jpeg",
            enable_motion_detection=True
        )
        
        # Préparer la requête
        request = vision_pb2.StreamRequest(
            camera_id="test_camera_001",
            camera_url="/workspaces/surveillance-system/vision-service/test_video.mp4",
            config=config
        )
        
        try:
            response = stub.StartStream(request)
            print(f"\n✅ Stream Started!")
            print(f"   Status: {response.status}")
            print(f"   Message: {response.message}")
            print(f"   Stream ID: {response.stream_id}")
            return True
        except Exception as e:
            print(f"\n❌ StartStream failed: {e}")
            return False

def test_get_status():
    """Test du statut du stream"""
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = vision_pb2_grpc.VisionServiceStub(channel)
        request = vision_pb2.StatusRequest(camera_id="test_camera_001")
        
        try:
            response = stub.GetStreamStatus(request)
            print(f"\n📊 Stream Status:")
            print(f"   Camera ID: {response.camera_id}")
            print(f"   Status: {response.status}")
            print(f"   Message: {response.message}")
            if response.HasField('stats'):
                print(f"   FPS: {response.stats.fps}")
                print(f"   Frames processed: {response.stats.frames_processed}")
                print(f"   Detections: {response.stats.detections_count}")
            return True
        except Exception as e:
            print(f"\n❌ GetStatus failed: {e}")
            return False

def test_stop_stream():
    """Test de l'arrêt du stream"""
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = vision_pb2_grpc.VisionServiceStub(channel)
        request = vision_pb2.StopRequest(camera_id="test_camera_001")
        
        try:
            response = stub.StopStream(request)
            print(f"\n🛑 Stream Stopped!")
            print(f"   Status: {response.status}")
            print(f"   Message: {response.message}")
            return True
        except Exception as e:
            print(f"\n❌ StopStream failed: {e}")
            return False

if __name__ == "__main__":
    print("🧪 Testing gRPC Vision Service\n")
    print("=" * 50)
    
    # Test 1: Health check
    if not test_health():
        sys.exit(1)
    
    # Test 2: Start stream
    if not test_start_stream():
        sys.exit(1)
    
    # Attendre un peu pour le traitement
    import time
    print("\n⏳ Waiting 3 seconds for processing...")
    time.sleep(3)
    
    # Test 3: Get status
    test_get_status()
    
    # Attendre encore
    print("\n⏳ Waiting 5 more seconds...")
    time.sleep(5)
    
    # Test 4: Get status again
    test_get_status()
    
    # Test 5: Stop stream
    test_stop_stream()
    
    print("\n" + "=" * 50)
    print("✅ All tests completed!")

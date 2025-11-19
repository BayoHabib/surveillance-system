// Test d'ajout de caméra RTSP via API
// Ouvrir la console du navigateur (F12) et exécuter ce code

async function testRTSPCamera() {
    const rtspUrl = 'rtsp://localhost:8554/test';
    
    console.log('🎥 Testing RTSP camera:', rtspUrl);
    
    try {
        // 1. Ajouter la caméra
        const response = await fetch('/api/v1/cameras', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                name: 'Test RTSP Camera',
                url: rtspUrl,
                type: 'rtsp',
                enabled: true
            })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${await response.text()}`);
        }
        
        const camera = await response.json();
        console.log('✅ Camera added:', camera);
        
        // 2. Démarrer le stream
        const startResponse = await fetch(`/api/v1/cameras/${camera.id}/start`, {
            method: 'PUT'
        });
        
        if (!startResponse.ok) {
            throw new Error(`Start failed: ${await startResponse.text()}`);
        }
        
        console.log('✅ Stream started!');
        console.log('📺 Stream URL:', `/api/v1/cameras/${camera.id}/stream`);
        console.log('🔌 WebSocket:', `/api/v1/cameras/${camera.id}/ws-stream`);
        
        // 3. Vérifier le statut après 3 secondes
        setTimeout(async () => {
            const statusResponse = await fetch(`/api/v1/cameras/${camera.id}`);
            const status = await statusResponse.json();
            console.log('📊 Camera status:', status);
            
            if (status.status === 'active') {
                console.log('🎉 RTSP TCP FIX WORKING!');
            } else {
                console.log('⚠️ Camera status:', status.status);
                console.log('💡 Check vision-service logs for details');
            }
        }, 3000);
        
        return camera;
        
    } catch (error) {
        console.error('❌ Error:', error);
        throw error;
    }
}

// Exécuter le test
testRTSPCamera()
    .then(camera => console.log('Test completed for camera:', camera.id))
    .catch(err => console.error('Test failed:', err));

Write-Host "Testing API endpoints..." -ForegroundColor Cyan

# Test health
Write-Host "`n1. Health Check:" -ForegroundColor Yellow
Invoke-RestMethod -Uri "http://localhost:8080/api/v1/health" | ConvertTo-Json

# Test cameras list
Write-Host "`n2. List Cameras:" -ForegroundColor Yellow
try {
    Invoke-RestMethod -Uri "http://localhost:8080/api/v1/cameras" | ConvertTo-Json -Depth 5
} catch {
    Write-Host "No cameras found or error: $_" -ForegroundColor Red
}

# Add a test camera with the video file
Write-Host "`n3. Adding Test Camera..." -ForegroundColor Yellow
$body = @{
    name = "Test Video Camera"
    url = "file:///mnt/c/Users/Administrator/git_clone/surveillance-system/test_camera_feed.mp4"
} | ConvertTo-Json

try {
    $result = Invoke-RestMethod -Uri "http://localhost:8080/api/v1/cameras/internet" `
        -Method POST `
        -ContentType "application/json" `
        -Body $body
    
    Write-Host "Camera added successfully!" -ForegroundColor Green
    $result | ConvertTo-Json -Depth 5
    
    Write-Host "`nWebSocket URL:" -ForegroundColor Cyan
    Write-Host "ws://localhost:8080/api/v1/cameras/$($result.camera.id)/ws-stream" -ForegroundColor Green
    
} catch {
    Write-Host "Error adding camera: $_" -ForegroundColor Red
}

Write-Host "`nDone!" -ForegroundColor Cyan

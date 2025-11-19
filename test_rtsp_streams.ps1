# Test de flux RTSP publics
# Liste de flux RTSP gratuits

$streams = @(
    @{
        name = "Big Buck Bunny (Wowza Demo)"
        url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4"
    },
    @{
        name = "Hessdalen Camera (Norway)"
        url = "rtsp://freja.hiof.no:1935/rtplive/_definst_/hessdalen03.stream"
    },
    @{
        name = "RTSP Test Pattern"
        url = "rtsp://rtsp.stream/pattern"
    }
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Testing Public RTSP Streams" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

foreach ($stream in $streams) {
    Write-Host "Testing: $($stream.name)" -ForegroundColor Yellow
    Write-Host "URL: $($stream.url)" -ForegroundColor Gray
    
    # Test avec FFmpeg (doit être installé sur Windows)
    $result = wsl bash -c "timeout 5 ffmpeg -rtsp_transport tcp -i '$($stream.url)' -frames:v 1 -f null - 2>&1" 2>&1 | Out-String
    
    if ($result -match "frame=\s*1") {
        Write-Host "  ✓ SUCCESS - Stream accessible!" -ForegroundColor Green
        Write-Host ""
        Write-Host "🎉 Found working stream: $($stream.url)" -ForegroundColor Green
        Write-Host ""
        Write-Host "Test with vision-service:" -ForegroundColor Cyan
        Write-Host "  wsl bash -c 'cd /mnt/c/Users/Administrator/git_clone/surveillance-system/vision-service && ./test_tcp_fix ""$($stream.url)""'" -ForegroundColor White
        break
    } else {
        Write-Host "  ✗ Failed or timeout" -ForegroundColor Red
    }
    Write-Host ""
}

Write-Host ""
Write-Host "Alternative: Use local test server" -ForegroundColor Yellow
Write-Host "  1. Start: docker run -d -p 8554:8554 bluenviron/mediamtx" -ForegroundColor Gray
Write-Host "  2. Publish: ffmpeg -re -f lavfi -i testsrc -f rtsp rtsp://localhost:8554/test" -ForegroundColor Gray
Write-Host "  3. Test: ./test_tcp_fix rtsp://localhost:8554/test" -ForegroundColor Gray

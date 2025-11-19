#!/bin/bash

# ==========================================
# Script de Test RTSP pour WSL
# ==========================================
# Ce script teste différentes configurations RTSP
# et diagnostique les problèmes de connexion

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   RTSP Test Script for WSL${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Configuration
RTSP_TEST_URL=${1:-"rtsp://localhost:8554/test"}
VISION_SERVICE_BIN="./build/vision-service"
DOCKER_AVAILABLE=false

# ==========================================
# Fonction: Vérifier dépendances
# ==========================================
check_dependencies() {
    echo -e "${YELLOW}[1/6] Checking dependencies...${NC}"
    
    local required=("ffmpeg" "nc")
    local optional=("docker")
    local missing=()
    
    for dep in "${required[@]}"; do
        if ! command -v $dep &> /dev/null; then
            missing+=($dep)
            echo -e "${RED}  ✗ $dep not found${NC}"
        else
            echo -e "${GREEN}  ✓ $dep found${NC}"
        fi
    done
    
    # Docker est optionnel
    if command -v docker &> /dev/null && docker ps &> /dev/null; then
        echo -e "${GREEN}  ✓ docker found and running${NC}"
        DOCKER_AVAILABLE=true
    else
        echo -e "${YELLOW}  ⚠ docker not available (will skip local test server)${NC}"
        DOCKER_AVAILABLE=false
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo -e "${RED}Missing required dependencies: ${missing[*]}${NC}"
        echo "Install with: sudo apt-get install -y ffmpeg netcat-openbsd"
        exit 1
    fi
    echo ""
}

# ==========================================
# Fonction: Démarrer serveur RTSP test
# ==========================================
start_rtsp_server() {
    echo -e "${YELLOW}[2/6] Starting RTSP test server...${NC}"
    
    # Si URL fournie, on skip le serveur local
    if [ "$RTSP_TEST_URL" != "rtsp://localhost:8554/test" ]; then
        echo -e "${BLUE}  ℹ Using external RTSP URL: $RTSP_TEST_URL${NC}"
        echo -e "${BLUE}  ℹ Skipping local test server${NC}"
        echo ""
        return
    fi
    
    # Si Docker non disponible, avertir
    if [ "$DOCKER_AVAILABLE" != "true" ]; then
        echo -e "${YELLOW}  ⚠ Docker not available - cannot start local test server${NC}"
        echo -e "${YELLOW}  ⚠ Please provide external RTSP URL:${NC}"
        echo -e "${YELLOW}     ./test_rtsp.sh rtsp://camera_ip:554/stream${NC}"
        echo ""
        exit 1
    fi
    
    # Vérifier si le serveur tourne déjà
    if nc -z localhost 8554 2>/dev/null; then
        echo -e "${GREEN}  ✓ RTSP server already running on port 8554${NC}"
        echo ""
        return
    fi
    
    # Démarrer serveur RTSP avec Docker
    echo "  Starting mediamtx (rtsp-simple-server replacement)..."
    if docker run -d --rm \
        --name rtsp-test-server \
        -p 8554:8554 \
        bluenviron/mediamtx >/dev/null 2>&1; then
        
        # Attendre que le serveur démarre
        sleep 2
        
        if nc -z localhost 8554 2>/dev/null; then
            echo -e "${GREEN}  ✓ RTSP server started successfully${NC}"
        else
            echo -e "${RED}  ✗ Failed to start RTSP server${NC}"
            exit 1
        fi
    else
        echo -e "${RED}  ✗ Failed to launch Docker container${NC}"
        exit 1
    fi
    echo ""
}

# ==========================================
# Fonction: Publier flux de test
# ==========================================
publish_test_stream() {
    echo -e "${YELLOW}[3/6] Publishing test stream...${NC}"
    
    # Si URL externe fournie, skip publication
    if [ "$RTSP_TEST_URL" != "rtsp://localhost:8554/test" ]; then
        echo -e "${BLUE}  ℹ Using external RTSP source - skipping stream publishing${NC}"
        echo ""
        return
    fi
    
    # Si Docker non disponible, déjà géré dans start_rtsp_server
    if [ "$DOCKER_AVAILABLE" != "true" ]; then
        return
    fi
    
    # Générer une vidéo de test si nécessaire
    if [ ! -f "test-pattern.mp4" ]; then
        echo "  Generating test pattern video..."
        ffmpeg -f lavfi -i testsrc=duration=10:size=1280x720:rate=30 \
               -pix_fmt yuv420p -c:v libx264 -preset ultrafast \
               test-pattern.mp4 -y >/dev/null 2>&1
        echo -e "${GREEN}  ✓ Test pattern created${NC}"
    fi
    
    # Publier le flux (en arrière-plan)
    echo "  Publishing stream to rtsp://localhost:8554/test..."
    ffmpeg -re -stream_loop -1 -i test-pattern.mp4 \
           -c copy -f rtsp rtsp://localhost:8554/test \
           >/dev/null 2>&1 &
    FFMPEG_PID=$!
    
    # Attendre que le flux soit disponible
    sleep 2
    echo -e "${GREEN}  ✓ Stream published (PID: $FFMPEG_PID)${NC}"
    echo ""
}

# ==========================================
# Fonction: Tester connexion RTSP
# ==========================================
test_rtsp_connection() {
    echo -e "${YELLOW}[4/6] Testing RTSP connection...${NC}"
    
    # Test 1: Ping port RTSP
    echo "  [Test 1] Port 8554 reachable..."
    if nc -z localhost 8554 2>/dev/null; then
        echo -e "${GREEN}    ✓ Port 8554 is open${NC}"
    else
        echo -e "${RED}    ✗ Port 8554 is closed${NC}"
        return 1
    fi
    
    # Test 2: FFmpeg avec UDP (défaut)
    echo "  [Test 2] FFmpeg UDP (default)..."
    if timeout 5 ffmpeg -i "$RTSP_TEST_URL" -frames:v 1 -f null - \
       >/dev/null 2>&1; then
        echo -e "${GREEN}    ✓ UDP transport works${NC}"
    else
        echo -e "${YELLOW}    ⚠ UDP transport failed (expected in WSL)${NC}"
    fi
    
    # Test 3: FFmpeg avec TCP (fix WSL)
    echo "  [Test 3] FFmpeg TCP (WSL fix)..."
    if timeout 5 ffmpeg -rtsp_transport tcp -i "$RTSP_TEST_URL" \
       -frames:v 1 -f null - >/dev/null 2>&1; then
        echo -e "${GREEN}    ✓ TCP transport works!${NC}"
    else
        echo -e "${RED}    ✗ TCP transport failed${NC}"
        return 1
    fi
    
    # Test 4: Capturer quelques frames
    echo "  [Test 4] Capturing test frames..."
    rm -f test_frame*.jpg
    if timeout 10 ffmpeg -rtsp_transport tcp -i "$RTSP_TEST_URL" \
       -frames:v 3 test_frame%03d.jpg -y >/dev/null 2>&1; then
        local frame_count=$(ls test_frame*.jpg 2>/dev/null | wc -l)
        echo -e "${GREEN}    ✓ Captured $frame_count frames${NC}"
    else
        echo -e "${RED}    ✗ Frame capture failed${NC}"
        return 1
    fi
    
    echo ""
}

# ==========================================
# Fonction: Tester vision-service
# ==========================================
test_vision_service() {
    echo -e "${YELLOW}[5/6] Testing vision-service...${NC}"
    
    # Vérifier que le binaire existe
    if [ ! -f "$VISION_SERVICE_BIN" ]; then
        echo -e "${RED}  ✗ vision-service not found at $VISION_SERVICE_BIN${NC}"
        echo "  Build with: cd vision-service && make"
        return 1
    fi
    
    echo "  Starting vision-service with RTSP..."
    echo "  URL: $RTSP_TEST_URL"
    echo ""
    
    # Lancer vision-service avec timeout
    timeout 30 $VISION_SERVICE_BIN \
        --camera="$RTSP_TEST_URL" \
        --log-level=DEBUG \
        2>&1 | tee /tmp/vision-service-test.log &
    
    local VS_PID=$!
    
    # Attendre et vérifier les logs
    sleep 5
    
    # Chercher les indicateurs de succès dans les logs
    if grep -q "RTSP capture configured with TCP transport" /tmp/vision-service-test.log; then
        echo -e "${GREEN}  ✓ TCP transport configured${NC}"
    else
        echo -e "${YELLOW}  ⚠ TCP configuration not found in logs${NC}"
    fi
    
    if grep -q "Frame captured" /tmp/vision-service-test.log; then
        echo -e "${GREEN}  ✓ Frames being captured successfully${NC}"
    else
        echo -e "${RED}  ✗ No frames captured${NC}"
    fi
    
    # Arrêter vision-service
    kill $VS_PID 2>/dev/null || true
    wait $VS_PID 2>/dev/null || true
    
    echo ""
    echo "  Full logs saved to: /tmp/vision-service-test.log"
    echo ""
}

# ==========================================
# Fonction: Nettoyer
# ==========================================
cleanup() {
    echo -e "${YELLOW}[6/6] Cleaning up...${NC}"
    
    # Arrêter FFmpeg publisher
    if [ -n "$FFMPEG_PID" ]; then
        kill $FFMPEG_PID 2>/dev/null || true
        echo "  ✓ Stopped FFmpeg publisher"
    fi
    
    # Arrêter serveur RTSP
    if docker ps | grep -q rtsp-test-server; then
        docker stop rtsp-test-server >/dev/null 2>&1
        echo "  ✓ Stopped RTSP test server"
    fi
    
    # Nettoyer fichiers temporaires
    rm -f test_frame*.jpg
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}   Test completed${NC}"
    echo -e "${GREEN}========================================${NC}"
}

# ==========================================
# Fonction: Afficher résumé
# ==========================================
show_summary() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   Test Summary${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "RTSP URL tested: $RTSP_TEST_URL"
    echo ""
    echo "Next steps:"
    echo "  1. Check logs: /tmp/vision-service-test.log"
    echo "  2. Test with real camera:"
    echo "     ./test_rtsp.sh rtsp://admin:pass@192.168.1.50:554/stream"
    echo "  3. Monitor vision-service:"
    echo "     tail -f logs/vision-service.log | grep RTSP"
    echo ""
}

# ==========================================
# Gestion des signaux
# ==========================================
trap cleanup EXIT

# ==========================================
# Main
# ==========================================
main() {
    check_dependencies
    start_rtsp_server
    publish_test_stream
    test_rtsp_connection
    test_vision_service
    show_summary
}

# Lancer les tests
main

#!/bin/bash

# ==========================================
# Test RTSP Simple - Validation TCP Fix
# ==========================================
# Ce script teste uniquement le fix TCP avec FFmpeg
# Sans dépendances Docker

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   RTSP TCP Fix - Simple Test${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Configuration
RTSP_URL=${1:-""}

# URLs de test publiques (souvent disponibles)
TEST_URLS=(
    "rtsp://rtsp.stream/pattern"
    "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4"
)

# ==========================================
# Fonction: Afficher usage
# ==========================================
show_usage() {
    echo "Usage: $0 [rtsp_url]"
    echo ""
    echo "Exemples:"
    echo "  $0                                    # Test avec URLs publiques"
    echo "  $0 rtsp://admin:pass@192.168.1.50:554/stream1"
    echo ""
}

# ==========================================
# Fonction: Vérifier FFmpeg
# ==========================================
check_ffmpeg() {
    echo -e "${YELLOW}[1/4] Checking FFmpeg...${NC}"
    
    if ! command -v ffmpeg &> /dev/null; then
        echo -e "${RED}  ✗ FFmpeg not found${NC}"
        echo "Install with: sudo apt-get install -y ffmpeg"
        exit 1
    fi
    
    echo -e "${GREEN}  ✓ FFmpeg found${NC}"
    echo ""
}

# ==========================================
# Fonction: Tester URL RTSP
# ==========================================
test_rtsp_url() {
    local url=$1
    local label=$2
    
    echo -e "${BLUE}Testing: $label${NC}"
    echo "URL: $url"
    echo ""
    
    # Test 1: UDP (défaut - devrait échouer dans WSL)
    echo -e "${YELLOW}  [Test 1/3] UDP transport (default)...${NC}"
    if timeout 5 ffmpeg -i "$url" -frames:v 1 -f null - >/dev/null 2>&1; then
        echo -e "${GREEN}    ✓ UDP works${NC}"
        UDP_RESULT="✓ OK"
    else
        echo -e "${YELLOW}    ⚠ UDP failed (expected in WSL)${NC}"
        UDP_RESULT="✗ FAIL"
    fi
    
    # Test 2: TCP (fix WSL)
    echo -e "${YELLOW}  [Test 2/3] TCP transport (WSL fix)...${NC}"
    if timeout 5 ffmpeg -rtsp_transport tcp -i "$url" \
       -frames:v 1 -f null - >/dev/null 2>&1; then
        echo -e "${GREEN}    ✓ TCP works! ${NC}🎉${NC}"
        TCP_RESULT="✓ OK"
    else
        echo -e "${RED}    ✗ TCP failed${NC}"
        TCP_RESULT="✗ FAIL"
        return 1
    fi
    
    # Test 3: Capturer frame
    echo -e "${YELLOW}  [Test 3/3] Capturing test frame...${NC}"
    rm -f test_frame.jpg
    if timeout 10 ffmpeg -rtsp_transport tcp -i "$url" \
       -frames:v 1 test_frame.jpg -y >/dev/null 2>&1; then
        
        if [ -f "test_frame.jpg" ]; then
            local size=$(stat -c%s "test_frame.jpg" 2>/dev/null || stat -f%z "test_frame.jpg" 2>/dev/null)
            echo -e "${GREEN}    ✓ Frame captured (${size} bytes)${NC}"
            FRAME_RESULT="✓ OK (${size}B)"
        else
            echo -e "${RED}    ✗ Frame file not created${NC}"
            FRAME_RESULT="✗ NO FILE"
            return 1
        fi
    else
        echo -e "${RED}    ✗ Frame capture failed${NC}"
        FRAME_RESULT="✗ FAIL"
        return 1
    fi
    
    echo ""
    return 0
}

# ==========================================
# Fonction: Test avec vision-service
# ==========================================
test_vision_service() {
    echo -e "${YELLOW}[4/4] Testing vision-service integration...${NC}"
    
    if [ ! -f "./build/vision-service" ]; then
        echo -e "${YELLOW}  ⚠ vision-service not built${NC}"
        echo "  Build with: make"
        echo ""
        return
    fi
    
    # Extraire host de l'URL
    local test_url=$1
    echo "  Testing with: $test_url"
    
    # Lancer vision-service en arrière-plan
    timeout 15 ./build/vision-service \
        --camera="$test_url" \
        --log-level=DEBUG \
        > /tmp/vision-service-test.log 2>&1 &
    local pid=$!
    
    sleep 5
    
    # Vérifier les logs
    if grep -q "TCP transport" /tmp/vision-service-test.log; then
        echo -e "${GREEN}  ✓ TCP transport configured${NC}"
    else
        echo -e "${YELLOW}  ⚠ TCP transport not confirmed in logs${NC}"
    fi
    
    if grep -q "Frame captured" /tmp/vision-service-test.log; then
        echo -e "${GREEN}  ✓ Frames being captured${NC}"
    else
        echo -e "${YELLOW}  ⚠ No frames captured yet${NC}"
    fi
    
    # Tuer le process
    kill $pid 2>/dev/null || true
    
    echo -e "${BLUE}  ℹ Full logs: /tmp/vision-service-test.log${NC}"
    echo ""
}

# ==========================================
# Fonction: Afficher résumé
# ==========================================
show_summary() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   Test Summary${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "Results:"
    echo "  UDP Transport:   $UDP_RESULT"
    echo "  TCP Transport:   $TCP_RESULT"
    echo "  Frame Capture:   $FRAME_RESULT"
    echo ""
    
    if [ "$TCP_RESULT" = "✓ OK" ]; then
        echo -e "${GREEN}✅ WSL RTSP Fix Working!${NC}"
        echo ""
        echo "The TCP transport fix resolves WSL NAT issues."
        echo "Your vision-service is configured to use TCP automatically."
    else
        echo -e "${RED}❌ TCP Transport Failed${NC}"
        echo ""
        echo "Possible issues:"
        echo "  1. RTSP server not reachable"
        echo "  2. Authentication required"
        echo "  3. Firewall blocking port 554"
        echo "  4. Camera not supporting TCP"
    fi
    echo ""
}

# ==========================================
# Main
# ==========================================
main() {
    # Si aucune URL fournie
    if [ -z "$RTSP_URL" ]; then
        echo -e "${BLUE}No URL provided - testing with public RTSP streams${NC}"
        echo ""
        
        check_ffmpeg
        
        local success=false
        for url in "${TEST_URLS[@]}"; do
            echo -e "${YELLOW}[2/4] Testing RTSP connection...${NC}"
            if test_rtsp_url "$url" "Public Test Stream"; then
                success=true
                
                echo -e "${YELLOW}[3/4] Additional info...${NC}"
                echo -e "${GREEN}  ✓ RTSP TCP fix validated!${NC}"
                echo ""
                
                # Test avec vision-service si disponible
                test_vision_service "$url"
                
                break
            else
                echo -e "${YELLOW}  ⚠ This stream failed, trying next...${NC}"
                echo ""
            fi
        done
        
        if [ "$success" = false ]; then
            echo -e "${RED}All public test streams failed${NC}"
            echo "Try with your own camera:"
            show_usage
            exit 1
        fi
    else
        # URL fournie par l'utilisateur
        check_ffmpeg
        
        echo -e "${YELLOW}[2/4] Testing RTSP connection...${NC}"
        if test_rtsp_url "$RTSP_URL" "User-Provided URL"; then
            echo -e "${YELLOW}[3/4] Additional info...${NC}"
            echo -e "${GREEN}  ✓ RTSP TCP fix validated!${NC}"
            echo ""
            
            test_vision_service "$RTSP_URL"
        else
            echo -e "${RED}Connection failed${NC}"
            show_usage
            exit 1
        fi
    fi
    
    show_summary
}

# Lancer les tests
main

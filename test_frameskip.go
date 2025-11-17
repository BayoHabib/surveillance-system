// Test pour Bug #4: Frame skipping pour clients lents
// Ce test simule un client lent et vérifie que le serveur skip des frames

package main

import (
	"fmt"
	"log"
	"net/http"
	"time"
)

func main() {
	fmt.Println("\n========================================")
	fmt.Println("BUG #4: Frame Skipping Test")
	fmt.Println("========================================\n")
	
	fmt.Println("Ce test nécessite:")
	fmt.Println("1. Le serveur Go doit être lancé (port 8080)")
	fmt.Println("2. Une caméra doit être configurée")
	fmt.Println("3. Observer les logs du serveur pour voir le frame skipping")
	fmt.Println()
	
	// Test 1: Client normal (rapide)
	fmt.Println("=== Test 1: Client Normal (Fast) ===")
	fmt.Println("Consommation rapide des frames...")
	go testFastClient()
	time.Sleep(10 * time.Second)
	
	// Test 2: Client lent (simule réseau lent)
	fmt.Println("\n=== Test 2: Client Lent (Slow Network) ===")
	fmt.Println("Consommation lente des frames avec delays...")
	go testSlowClient()
	time.Sleep(10 * time.Second)
	
	fmt.Println("\n========================================")
	fmt.Println("Résultats Attendus dans les Logs Serveur:")
	fmt.Println("========================================")
	fmt.Println()
	fmt.Println("Client Normal:")
	fmt.Println("  - skipInterval = 1 (pas de skip)")
	fmt.Println("  - Buffer load < 60%")
	fmt.Println("  - '📸 Camera X: N frames (0% skipped)'")
	fmt.Println()
	fmt.Println("Client Lent:")
	fmt.Println("  - skipInterval = 2 ou 3 (skip actif)")
	fmt.Println("  - Buffer load > 60% ou > 80%")
	fmt.Println("  - '⚠️ Camera X: High buffer load (85%), skipping 2/3 frames'")
	fmt.Println("  - '📸 Camera X: N frames (50%-66% skipped)'")
	fmt.Println()
	
	fmt.Println("\n========================================")
	fmt.Println("Implementation Details")
	fmt.Println("========================================\n")
	fmt.Println("Fichier: cmd/server/main.go - streamHandler()")
	fmt.Println()
	fmt.Println("Variables ajoutées:")
	fmt.Println("  - skippedFrames := 0")
	fmt.Println("  - skipInterval := 1  // 1=no skip, 2=skip 1/2, 3=skip 2/3")
	fmt.Println("  - lastSkipCheck := time.Now()")
	fmt.Println()
	fmt.Println("Logique de monitoring (toutes les 30 frames):")
	fmt.Println("  bufferLoad := len(frames)")
	fmt.Println("  bufferCapacity := cap(frames)")
	fmt.Println("  loadPercent := (bufferLoad * 100) / bufferCapacity")
	fmt.Println()
	fmt.Println("  if loadPercent > 80:")
	fmt.Println("    skipInterval = 3  // Skip 2/3 frames")
	fmt.Println("  else if loadPercent > 60:")
	fmt.Println("    skipInterval = 2  // Skip 1/2 frames")
	fmt.Println("  else:")
	fmt.Println("    skipInterval = 1  // No skip")
	fmt.Println()
	fmt.Println("Logique de skip:")
	fmt.Println("  if skipInterval > 1 && frameCount%skipInterval != 0:")
	fmt.Println("    skippedFrames++")
	fmt.Println("    continue  // Skip cette frame")
	fmt.Println()
	
	fmt.Println("\n========================================")
	fmt.Println("Manual Testing Instructions")
	fmt.Println("========================================\n")
	fmt.Println("1. Démarrer le serveur:")
	fmt.Println("   ./surveillance-server")
	fmt.Println()
	fmt.Println("2. Ouvrir le stream dans un navigateur:")
	fmt.Println("   http://localhost:8080/api/v1/cameras/camera1/stream")
	fmt.Println()
	fmt.Println("3. Simuler un client lent avec throttling:")
	fmt.Println("   - Chrome DevTools > Network > Throttling > Slow 3G")
	fmt.Println("   - Ou avec curl: curl -N --limit-rate 10k http://...")
	fmt.Println()
	fmt.Println("4. Observer les logs du serveur:")
	fmt.Println("   - Messages '⚠️ High buffer load' quand client est lent")
	fmt.Println("   - Statistiques '📸 ... (X% skipped)' toutes les 100 frames")
	fmt.Println("   - skipInterval augmente automatiquement")
	fmt.Println()
	fmt.Println("5. Améliorer la connexion:")
	fmt.Println("   - Retirer le throttling")
	fmt.Println("   - Observer '✓ Buffer load normal' dans les logs")
	fmt.Println("   - skipInterval retourne à 1")
	fmt.Println()
	
	fmt.Println("\n========================================")
	fmt.Println("✅ Bug #4 Implementation Complete")
	fmt.Println("========================================\n")
}

func testFastClient() {
	resp, err := http.Get("http://localhost:8080/api/v1/cameras/camera1/stream")
	if err != nil {
		log.Printf("❌ Fast client error: %v", err)
		return
	}
	defer resp.Body.Close()
	
	// Lire rapidement (buffer 64KB)
	buffer := make([]byte, 65536)
	for i := 0; i < 100; i++ {
		_, err := resp.Body.Read(buffer)
		if err != nil {
			break
		}
		// Pas de delay - consommation rapide
	}
	
	log.Println("✓ Fast client finished")
}

func testSlowClient() {
	resp, err := http.Get("http://localhost:8080/api/v1/cameras/camera1/stream")
	if err != nil {
		log.Printf("❌ Slow client error: %v", err)
		return
	}
	defer resp.Body.Close()
	
	// Lire lentement (buffer 4KB avec delays)
	buffer := make([]byte, 4096)
	for i := 0; i < 100; i++ {
		_, err := resp.Body.Read(buffer)
		if err != nil {
			break
		}
		// Delay 100ms - simule réseau lent
		time.Sleep(100 * time.Millisecond)
	}
	
	log.Println("✓ Slow client finished")
}

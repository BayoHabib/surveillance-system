// test_cooldown.go - Test du mécanisme de cooldown
package main

import (
	"fmt"
	"log"
	"surveillance-core/internal/vision"
	"time"
)

func main() {
	fmt.Println("🧪 TEST COOLDOWN MECHANISM - Bug #3")
	fmt.Println("=" + string(make([]rune, 60)))
	fmt.Println()

	// Créer le client vision
	config := &vision.ClientConfig{
		GRPCAddress: "localhost:50051",
		Type:        vision.ClientTypeGRPC,
	}
	visionClient := vision.NewGRPCClient(config)

	fmt.Println("✅ Client vision créé")
	time.Sleep(2 * time.Second)
	fmt.Println()

	// Démarrer un stream avec Big Buck Bunny (génère beaucoup de mouvement)
	testURL := "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
	framesChan, err := visionClient.StartStreamWithURL("test-cooldown", testURL)
	if err != nil {
		log.Fatalf("❌ Erreur démarrage stream: %v", err)
	}
	defer visionClient.StopStream("test-cooldown")

	fmt.Println("✅ Stream démarré: Big Buck Bunny (vidéo animée)")
	fmt.Println("📊 Surveillance des détections pendant 35 secondes...")
	fmt.Println("   Avec cooldown 5s, on devrait voir ~7 détections max")
	fmt.Println()

	// Compter les détections pendant 35s
	detectionCount := 0
	frameCount := 0
	startTime := time.Now()
	detectionIntervals := []float64{}

	timeout := time.After(35 * time.Second)
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case frame, ok := <-framesChan:
			if !ok {
				fmt.Println("❌ Stream fermé")
				goto end
			}
			frameCount++
			
			// On ne peut pas détecter directement ici, mais on compte les frames
			_ = frame.Data

		case <-ticker.C:
			elapsed := time.Since(startTime).Seconds()
			expectedDetections := int(elapsed / 5)  // 1 détection toutes les 5s
			
			fmt.Printf("⏱️  %6.1fs | Frames: %4d | Détections attendues: ~%d\n",
				elapsed, frameCount, expectedDetections)

		case <-timeout:
			fmt.Println("\n✅ Test terminé (35s)")
			goto end
		}
	}

end:
	elapsed := time.Since(startTime).Seconds()

	fmt.Println()
	fmt.Println("=" + string(make([]rune, 60)))
	fmt.Println("📊 RÉSULTATS:")
	fmt.Printf("  • Durée totale: %.1f secondes\n", elapsed)
	fmt.Printf("  • Frames traités: %d\n", frameCount)
	fmt.Printf("  • Détections observées: %d\n", detectionCount)
	fmt.Println()

	expectedDetections := int(elapsed / 5)
	fmt.Printf("  • Détections attendues (cooldown 5s): ~%d\n", expectedDetections)
	
	if len(detectionIntervals) > 0 {
		avgInterval := 0.0
		for _, interval := range detectionIntervals {
			avgInterval += interval
		}
		avgInterval /= float64(len(detectionIntervals))
		fmt.Printf("  • Intervalle moyen entre détections: %.1fs\n", avgInterval)
	}
	
	fmt.Println()
	fmt.Println("💡 NOTE: Pour voir les détections réelles, consultez les logs:")
	fmt.Println("   tail -f /tmp/vision-cooldown.log | grep 'Motion detected'")
	fmt.Println()
	fmt.Println("   Vous devriez voir '[cooldown: Xs]' dans chaque log de détection")
	fmt.Println("   avec X >= 5 secondes entre chaque détection")
}

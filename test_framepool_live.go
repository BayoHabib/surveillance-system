// test_framepool_live.go - Test du Frame Pool en conditions réelles
package main

import (
	"fmt"
	"log"
	"surveillance-core/internal/core"
	"surveillance-core/internal/vision"
	"time"
)

func main() {
	fmt.Println("🧪 TEST FRAME POOL - Conditions réelles")
	fmt.Println("=" + string(make([]rune, 60)))
	fmt.Println()

	// Créer le client vision
	config := &vision.ClientConfig{
		GRPCAddress: "localhost:50051",
		Type:        vision.ClientTypeGRPC,
	}
	visionClient := vision.NewGRPCClient(config)

	fmt.Println("✅ Client vision créé")
	time.Sleep(2 * time.Second) // Attendre connexion
	fmt.Println()

	// Démarrer un stream avec URL internet
	testURL := "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
	framesChan, err := visionClient.StartStreamWithURL("test-camera-internet", testURL)
	if err != nil {
		log.Fatalf("❌ Erreur démarrage stream: %v", err)
	}
	defer visionClient.StopStream("test-camera-internet")

	fmt.Println("✅ Stream démarré: test-camera-internet (Big Buck Bunny)")
	fmt.Println("📊 Collecte des métriques pendant 30 secondes...")
	fmt.Println()

	// Consommer les frames pendant 30s
	frameCount := 0
	startTime := time.Now()
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	timeout := time.After(30 * time.Second)
	lastStats := time.Now()

	for {
		select {
		case frame, ok := <-framesChan:
			if !ok {
				fmt.Println("❌ Stream fermé")
				return
			}
			frameCount++
			
			// Simuler traitement (comme MJPEG encoding)
			_ = frame.Data
			
			// Stats toutes les 5s
			if time.Since(lastStats) >= 5*time.Second {
				elapsed := time.Since(startTime).Seconds()
				fps := float64(frameCount) / elapsed
				stats := core.GlobalFramePool.Stats()
				
				fmt.Printf("⏱️  %6.1fs | Frames: %4d | FPS: %5.1f | Pool: Alloc=%d Recycle=%d Reuse=%.1f%%\n",
					elapsed, frameCount, fps, stats.AllocCount, stats.RecycleCount, stats.ReuseRate)
				lastStats = time.Now()
			}

		case <-ticker.C:
			// Stats périodiques détaillées
			stats := core.GlobalFramePool.Stats()
			fmt.Printf("\n📊 STATS DÉTAILLÉES:\n")
			fmt.Printf("  • Allocations totales: %d\n", stats.AllocCount)
			fmt.Printf("  • Recyclages totaux: %d\n", stats.RecycleCount)
			fmt.Printf("  • Taux réutilisation: %.2f%%\n", stats.ReuseRate)
			fmt.Println()

		case <-timeout:
			fmt.Println("\n✅ Test terminé (30s)")
			goto end
		}
	}

end:
	// Stats finales
	elapsed := time.Since(startTime).Seconds()
	fps := float64(frameCount) / elapsed
	finalStats := core.GlobalFramePool.Stats()

	fmt.Println()
	fmt.Println("=" + string(make([]rune, 60)))
	fmt.Println("📊 RÉSULTATS FINAUX:")
	fmt.Printf("  • Durée totale: %.1f secondes\n", elapsed)
	fmt.Printf("  • Frames traités: %d\n", frameCount)
	fmt.Printf("  • FPS moyen: %.2f\n", fps)
	fmt.Println()
	fmt.Printf("  • Pool allocations: %d\n", finalStats.AllocCount)
	fmt.Printf("  • Pool recyclages: %d\n", finalStats.RecycleCount)
	fmt.Printf("  • Taux réutilisation: %.2f%%\n", finalStats.ReuseRate)
	fmt.Println()

	// Estimation mémoire économisée
	frameSize := 640 * 480 * 3 // bytes
	withoutPool := frameCount * frameSize
	withPool := int(finalStats.AllocCount) * frameSize
	saved := withoutPool - withPool
	
	fmt.Println("💾 IMPACT MÉMOIRE:")
	fmt.Printf("  • Sans pool: %d MB alloués\n", withoutPool/1024/1024)
	fmt.Printf("  • Avec pool: %d MB alloués\n", withPool/1024/1024)
	fmt.Printf("  • Économie: %d MB (%.1f%%)\n", saved/1024/1024, float64(saved)/float64(withoutPool)*100)
	fmt.Println()

	if finalStats.ReuseRate > 1000 {
		fmt.Println("🎉 EXCELLENT: Le pool recycle massivement!")
	} else if finalStats.ReuseRate > 100 {
		fmt.Println("✅ BON: Le pool fonctionne correctement")
	} else {
		fmt.Println("⚠️  ATTENTION: Faible taux de réutilisation")
	}
}

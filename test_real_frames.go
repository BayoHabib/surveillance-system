// test_real_frames.go - Test des vraies frames via GetFrames RPC (Bug #1)
package main

import (
	"fmt"
	"log"
	"surveillance-core/internal/vision"
	"time"
)

func main() {
	fmt.Println("🧪 TEST REAL FRAMES via GetFrames RPC - Bug #1")
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

	// Démarrer un stream avec Big Buck Bunny
	testURL := "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
	framesChan, err := visionClient.StartStreamWithURL("test-real-frames", testURL)
	if err != nil {
		log.Fatalf("❌ Erreur démarrage stream: %v", err)
	}
	defer visionClient.StopStream("test-real-frames")

	fmt.Println("✅ Stream démarré avec URL Big Buck Bunny")
	fmt.Println("📊 Réception des vraies frames pendant 30 secondes...")
	fmt.Println()

	// Recevoir les frames pendant 30s
	frameCount := 0
	startTime := time.Now()
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	timeout := time.After(30 * time.Second)
	firstFrame := true
	var lastFrameSize int

	for {
		select {
		case frame, ok := <-framesChan:
			if !ok {
				fmt.Println("❌ Stream fermé")
				goto end
			}
			frameCount++
			
			// Analyser la première frame
			if firstFrame {
				fmt.Printf("📸 PREMIÈRE FRAME REÇUE:\n")
				fmt.Printf("  • Taille: %dx%d\n", frame.Width, frame.Height)
				fmt.Printf("  • Canaux: %d\n", frame.Channels)
				fmt.Printf("  • Format: %s\n", frame.Format)
				fmt.Printf("  • Data size: %d bytes\n", len(frame.Data))
				fmt.Printf("  • Camera ID: %s\n", frame.CameraID)
				fmt.Printf("  • Timestamp: %v\n", frame.Timestamp)
				
				// Vérifier que ce sont de vraies données (pas des mocks)
				allSame := true
				if len(frame.Data) > 100 {
					firstByte := frame.Data[0]
					for i := 1; i < 100; i++ {
						if frame.Data[i] != firstByte {
							allSame = false
							break
						}
					}
				}
				
				if allSame {
					fmt.Println("  ⚠️  WARNING: Les 100 premiers bytes sont identiques (possiblement mock)")
				} else {
					fmt.Println("  ✅ Données variées détectées (vraies frames!)")
				}
				fmt.Println()
				
				firstFrame = false
				lastFrameSize = len(frame.Data)
			}
			
			// Vérifier les changements de taille
			if len(frame.Data) != lastFrameSize && lastFrameSize > 0 {
				fmt.Printf("⚠️  Frame size changed: %d -> %d bytes\n", lastFrameSize, len(frame.Data))
				lastFrameSize = len(frame.Data)
			}

		case <-ticker.C:
			elapsed := time.Since(startTime).Seconds()
			fps := float64(frameCount) / elapsed
			fmt.Printf("⏱️  %6.1fs | Frames reçues: %4d | FPS: %5.1f\n", elapsed, frameCount, fps)

		case <-timeout:
			fmt.Println("\n✅ Test terminé (30s)")
			goto end
		}
	}

end:
	elapsed := time.Since(startTime).Seconds()
	fps := float64(frameCount) / elapsed

	fmt.Println()
	fmt.Println("=" + string(make([]rune, 60)))
	fmt.Println("📊 RÉSULTATS:")
	fmt.Printf("  • Durée totale: %.1f secondes\n", elapsed)
	fmt.Printf("  • Frames reçues: %d\n", frameCount)
	fmt.Printf("  • FPS moyen: %.2f\n", fps)
	fmt.Println()

	if frameCount == 0 {
		fmt.Println("❌ ÉCHEC: Aucune frame reçue!")
		fmt.Println("   Vérifier les logs du vision service: tail -f /tmp/vision-bug1.log")
	} else if frameCount < 100 {
		fmt.Println("⚠️  WARNING: Peu de frames reçues, vérifier les logs")
	} else {
		fmt.Println("🎉 SUCCESS: Frames reçues avec succès!")
		fmt.Println("   Bug #1 RÉSOLU: Transmission de vraies frames via gRPC")
	}
}

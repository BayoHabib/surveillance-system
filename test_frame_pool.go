// test_frame_pool.go - Test du Frame Pool
package main

import (
	"fmt"
	"runtime"
	"surveillance-core/internal/core"
	"time"
)

func main() {
	fmt.Println("🧪 TEST FRAME POOL - BUG #9")
	fmt.Println("=" + string(make([]rune, 50)))
	fmt.Println()

	// Test 1: Allocation/Recyclage basique
	fmt.Println("📊 Test 1: Allocation et recyclage basique")
	frame1 := core.GetFrame()
	fmt.Printf("  ✅ Frame obtenue (cap: %d bytes)\n", cap(frame1.Data))
	core.ReleaseFrame(frame1)
	fmt.Println("  ✅ Frame recyclée")
	fmt.Println()

	// Test 2: Multiple allocations
	fmt.Println("📊 Test 2: Multiple allocations (10 frames)")
	frames := make([]*core.Frame, 10)
	for i := 0; i < 10; i++ {
		frames[i] = core.GetFrameWithSize(640, 480, 3)
	}
	fmt.Println("  ✅ 10 frames allouées")
	
	for _, frame := range frames {
		core.ReleaseFrame(frame)
	}
	fmt.Println("  ✅ 10 frames recyclées")
	fmt.Println()

	// Test 3: Stats du pool
	fmt.Println("📊 Test 3: Statistiques du pool")
	stats := core.GlobalFramePool.Stats()
	fmt.Printf("  • Allocations: %d\n", stats.AllocCount)
	fmt.Printf("  • Recyclages: %d\n", stats.RecycleCount)
	fmt.Printf("  • Taux de réutilisation: %.2f%%\n", stats.ReuseRate)
	fmt.Println()

	// Test 4: Stress test (1000 frames)
	fmt.Println("📊 Test 4: Stress test (1000 frames en boucle)")
	startTime := time.Now()
	var memBefore runtime.MemStats
	runtime.ReadMemStats(&memBefore)
	
	for i := 0; i < 1000; i++ {
		frame := core.GetFrameWithSize(1280, 720, 3)
		// Simuler utilisation
		for j := 0; j < len(frame.Data); j += 100 {
			frame.Data[j] = byte(i % 255)
		}
		core.ReleaseFrame(frame)
	}
	
	elapsed := time.Since(startTime)
	var memAfter runtime.MemStats
	runtime.ReadMemStats(&memAfter)
	
	fmt.Printf("  • Durée: %v\n", elapsed)
	fmt.Printf("  • Mémoire avant: %d MB\n", memBefore.Alloc/1024/1024)
	fmt.Printf("  • Mémoire après: %d MB\n", memAfter.Alloc/1024/1024)
	fmt.Printf("  • Delta mémoire: %d MB\n", int64(memAfter.Alloc-memBefore.Alloc)/1024/1024)
	fmt.Println()

	// Stats finales
	finalStats := core.GlobalFramePool.Stats()
	fmt.Println("📊 STATISTIQUES FINALES:")
	fmt.Printf("  • Total allocations: %d\n", finalStats.AllocCount)
	fmt.Printf("  • Total recyclages: %d\n", finalStats.RecycleCount)
	fmt.Printf("  • Taux de réutilisation: %.2f%%\n", finalStats.ReuseRate)
	fmt.Println()

	// Comparaison avec allocation directe
	fmt.Println("📊 COMPARAISON: Pool vs Allocation directe")
	
	// Sans pool
	startNoPool := time.Now()
	for i := 0; i < 1000; i++ {
		data := make([]byte, 1280*720*3)
		_ = data // utilisation factice
	}
	noPoolTime := time.Since(startNoPool)
	
	// Avec pool
	startWithPool := time.Now()
	for i := 0; i < 1000; i++ {
		frame := core.GetFrameWithSize(1280, 720, 3)
		core.ReleaseFrame(frame)
	}
	withPoolTime := time.Since(startWithPool)
	
	fmt.Printf("  • Sans pool: %v\n", noPoolTime)
	fmt.Printf("  • Avec pool: %v\n", withPoolTime)
	improvement := float64(noPoolTime-withPoolTime) / float64(noPoolTime) * 100
	fmt.Printf("  • Amélioration: %.1f%%\n", improvement)
	fmt.Println()

	fmt.Println("✅ TESTS TERMINÉS")
	fmt.Println()
	
	if finalStats.ReuseRate > 80 {
		fmt.Println("🎉 SUCCESS: Taux de réutilisation > 80%")
	} else {
		fmt.Println("⚠️  WARNING: Taux de réutilisation faible")
	}
}

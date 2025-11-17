// internal/core/frame_pool.go
package core

import (
	"sync"
	"sync/atomic"
)

// FramePool gère un pool d'objets Frame pour éviter les allocations répétées
// BUG #9 Fix: Résout le memory leak causé par des allocations massives de frames
type FramePool struct {
	pool         sync.Pool
	allocCount   atomic.Int64
	recycleCount atomic.Int64
	defaultSize  int
}

// Statistiques du pool
type FramePoolStats struct {
	AllocCount   int64
	RecycleCount int64
	ReuseRate    float64
}

// DefaultFrameSize taille par défaut pour VGA (640x480x3)
const DefaultFrameSize = 640 * 480 * 3

// NewFramePool crée un nouveau pool de frames
func NewFramePool(defaultSize int) *FramePool {
	if defaultSize <= 0 {
		defaultSize = DefaultFrameSize
	}
	
	fp := &FramePool{
		defaultSize: defaultSize,
	}
	
	fp.pool = sync.Pool{
		New: func() interface{} {
			fp.allocCount.Add(1)
			return &Frame{
				Data: make([]byte, 0, fp.defaultSize),
			}
		},
	}
	
	return fp
}

// Get récupère une frame du pool
func (fp *FramePool) Get() *Frame {
	frame := fp.pool.Get().(*Frame)
	
	// Réinitialiser les champs
	frame.CameraID = ""
	frame.Data = frame.Data[:0]
	frame.Width = 0
	frame.Height = 0
	frame.Format = ""
	frame.Size = 0
	
	return frame
}

// Put retourne une frame au pool
func (fp *FramePool) Put(frame *Frame) {
	if frame == nil {
		return
	}
	
	// Ne recycler que si la capacité n'est pas trop grande
	// Évite de garder des buffers gigantesques en mémoire
	if cap(frame.Data) <= fp.defaultSize*4 {
		fp.recycleCount.Add(1)
		fp.pool.Put(frame)
	}
}

// GetWithSize récupère une frame avec une taille spécifique
func (fp *FramePool) GetWithSize(width, height, channels int) *Frame {
	frame := fp.Get()
	requiredSize := width * height * channels
	
	// Allouer plus de capacité si nécessaire
	if cap(frame.Data) < requiredSize {
		frame.Data = make([]byte, requiredSize)
	} else {
		frame.Data = frame.Data[:requiredSize]
	}
	
	frame.Width = width
	frame.Height = height
	frame.Size = requiredSize
	
	return frame
}

// Stats retourne les statistiques du pool
func (fp *FramePool) Stats() FramePoolStats {
	alloc := fp.allocCount.Load()
	recycle := fp.recycleCount.Load()
	
	var reuseRate float64
	if alloc > 0 {
		reuseRate = float64(recycle) / float64(alloc) * 100
	}
	
	return FramePoolStats{
		AllocCount:   alloc,
		RecycleCount: recycle,
		ReuseRate:    reuseRate,
	}
}

// Reset réinitialise les statistiques
func (fp *FramePool) Reset() {
	fp.allocCount.Store(0)
	fp.recycleCount.Store(0)
}

// GlobalFramePool instance globale du pool
var GlobalFramePool = NewFramePool(DefaultFrameSize)

// GetFrame récupère une frame du pool global
func GetFrame() *Frame {
	return GlobalFramePool.Get()
}

// ReleaseFrame retourne une frame au pool global
func ReleaseFrame(frame *Frame) {
	GlobalFramePool.Put(frame)
}

// GetFrameWithSize récupère une frame du pool global avec taille spécifique
func GetFrameWithSize(width, height, channels int) *Frame {
	return GlobalFramePool.GetWithSize(width, height, channels)
}

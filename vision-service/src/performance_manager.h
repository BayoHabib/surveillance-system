// src/performance_manager.h
// Framework de performance et threading pour Phase 2.3

#ifndef PERFORMANCE_MANAGER_H
#define PERFORMANCE_MANAGER_H

#include <opencv2/opencv.hpp>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <functional>

// Configuration de performance
struct PerformanceConfig {
    int max_threads = std::thread::hardware_concurrency();
    int max_queue_size = 100;
    bool enable_gpu = false;
    bool enable_memory_pool = true;
    size_t memory_pool_size_mb = 512;
    bool enable_profiling = false;
    int profiling_interval_ms = 1000;
};

// Métriques de performance détaillées
struct DetailedPerformanceMetrics {
    // Timing
    std::atomic<uint64_t> total_frames_processed{0};
    std::atomic<uint64_t> total_processing_time_us{0};
    std::atomic<uint64_t> total_queue_wait_time_us{0};
    std::atomic<uint64_t> total_gpu_time_us{0};
    
    // Throughput
    std::atomic<double> current_fps{0.0};
    std::atomic<double> avg_fps{0.0};
    std::atomic<double> peak_fps{0.0};
    
    // Memory
    std::atomic<uint64_t> current_memory_usage_mb{0};
    std::atomic<uint64_t> peak_memory_usage_mb{0};
    std::atomic<uint64_t> total_allocations{0};
    
    // Threading
    std::atomic<int> active_threads{0};
    std::atomic<int> queue_size{0};
    std::atomic<uint64_t> thread_context_switches{0};
    
    // Error tracking
    std::atomic<uint64_t> processing_errors{0};
    std::atomic<uint64_t> memory_allocation_failures{0};
    std::atomic<uint64_t> timeout_errors{0};
    
    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};
    
    // Méthodes utilitaires
    double GetAverageProcessingTimeMs() const {
        uint64_t frames = total_frames_processed.load();
        if (frames == 0) return 0.0;
        return static_cast<double>(total_processing_time_us.load()) / (frames * 1000.0);
    }
    
    double GetAverageQueueWaitTimeMs() const {
        uint64_t frames = total_frames_processed.load();
        if (frames == 0) return 0.0;
        return static_cast<double>(total_queue_wait_time_us.load()) / (frames * 1000.0);
    }
    
    double GetUptimeSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    }
};

// Gestionnaire de pool mémoire pour optimiser les allocations
class MemoryPool {
public:
    explicit MemoryPool(size_t pool_size_mb = 512);
    ~MemoryPool();
    
    // Allocation/désallocation
    void* Allocate(size_t size, size_t alignment = 8);
    void Deallocate(void* ptr);
    
    // Allocation spécialisée pour OpenCV Mat
    cv::Mat AllocateMat(int rows, int cols, int type);
    void DeallocateMat(cv::Mat& mat);
    
    // Statistiques
    size_t GetTotalSize() const { return pool_size_; }
    size_t GetUsedSize() const { return used_size_.load(); }
    size_t GetAvailableSize() const { return pool_size_ - used_size_.load(); }
    uint64_t GetAllocationCount() const { return allocation_count_.load(); }
    
    // Maintenance
    void Defragment();
    void Reset();
    
private:
    uint8_t* pool_memory_;
    size_t pool_size_;
    std::atomic<size_t> used_size_{0};
    std::atomic<uint64_t> allocation_count_{0};
    
    struct Block {
        void* ptr;
        size_t size;
        bool is_free;
        Block* next;
    };
    
    Block* free_blocks_;
    std::mutex pool_mutex_;
    
    Block* FindFreeBlock(size_t size, size_t alignment);
    void SplitBlock(Block* block, size_t size);
    void MergeBlocks();
};

// Thread pool optimisé pour le traitement vidéo
class VideoProcessingThreadPool {
public:
    explicit VideoProcessingThreadPool(int num_threads = std::thread::hardware_concurrency());
    ~VideoProcessingThreadPool();
    
    // Types de tâches
    using Task = std::function<void()>;
    using FrameProcessingTask = std::function<void(const cv::Mat&, cv::Mat&)>;
    using DetectionTask = std::function<std::vector<Detection>(const cv::Mat&)>;
    
    // Soumission de tâches
    std::future<void> SubmitTask(Task task);
    std::future<void> SubmitFrameProcessing(const cv::Mat& input, cv::Mat& output, FrameProcessingTask task);
    std::future<std::vector<Detection>> SubmitDetection(const cv::Mat& frame, DetectionTask task);
    
    // Gestion du pool
    void Start();
    void Stop();
    void WaitForAll();
    
    // Statistiques
    int GetActiveThreads() const { return active_threads_.load(); }
    int GetQueueSize() const { return queue_size_.load(); }
    uint64_t GetTasksCompleted() const { return tasks_completed_.load(); }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    
    std::atomic<int> active_threads_{0};
    std::atomic<int> queue_size_{0};
    std::atomic<uint64_t> tasks_completed_{0};
    
    void WorkerLoop();
};

// Profiler de performance pour mesurer les bottlenecks
class PerformanceProfiler {
public:
    explicit PerformanceProfiler(bool enabled = false);
    ~PerformanceProfiler();
    
    // Mesure de temps
    class ScopedTimer {
    public:
        ScopedTimer(PerformanceProfiler* profiler, const std::string& section);
        ~ScopedTimer();
        
    private:
        PerformanceProfiler* profiler_;
        std::string section_;
        std::chrono::high_resolution_clock::time_point start_time_;
    };
    
    // Macros pour faciliter l'utilisation
    #define PROFILE_SCOPE(profiler, name) \
        PerformanceProfiler::ScopedTimer timer(profiler, name)
    
    // Gestion des sections
    void StartSection(const std::string& name);
    void EndSection(const std::string& name);
    
    // Rapport de performance
    struct SectionStats {
        std::string name;
        uint64_t call_count = 0;
        uint64_t total_time_us = 0;
        uint64_t min_time_us = UINT64_MAX;
        uint64_t max_time_us = 0;
        double avg_time_us = 0.0;
        double percentage_of_total = 0.0;
    };
    
    std::vector<SectionStats> GetReport() const;
    void PrintReport() const;
    void Reset();
    
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }
    
private:
    bool enabled_;
    std::map<std::string, SectionStats> sections_;
    std::map<std::string, std::chrono::high_resolution_clock::time_point> active_timers_;
    mutable std::mutex profiler_mutex_;
    std::chrono::high_resolution_clock::time_point profiler_start_time_;
};

// Gestionnaire principal de performance
class PerformanceManager {
public:
    explicit PerformanceManager(const PerformanceConfig& config = PerformanceConfig{});
    ~PerformanceManager();
    
    // Lifecycle
    bool Initialize();
    void Shutdown();
    
    // Configuration
    void SetConfig(const PerformanceConfig& config);
    PerformanceConfig GetConfig() const;
    
    // Accès aux composants
    MemoryPool* GetMemoryPool() { return memory_pool_.get(); }
    VideoProcessingThreadPool* GetThreadPool() { return thread_pool_.get(); }
    PerformanceProfiler* GetProfiler() { return profiler_.get(); }
    
    // Métriques
    DetailedPerformanceMetrics GetMetrics() const;
    void UpdateMetrics(const std::string& operation, uint64_t duration_us, size_t memory_used = 0);
    void ResetMetrics();
    
    // Optimisations automatiques
    void EnableAutoOptimization(bool enable) { auto_optimization_ = enable; }
    void RunOptimizationPass();
    
    // GPU Support (si disponible)
    bool IsGPUAvailable() const;
    void EnableGPU(bool enable);
    cv::UMat CreateGPUMat(int rows, int cols, int type);
    
private:
    PerformanceConfig config_;
    DetailedPerformanceMetrics metrics_;
    
    std::unique_ptr<MemoryPool> memory_pool_;
    std::unique_ptr<VideoProcessingThreadPool> thread_pool_;
    std::unique_ptr<PerformanceProfiler> profiler_;
    
    bool initialized_;
    bool auto_optimization_;
    
    // Threads de monitoring
    std::thread metrics_thread_;
    std::atomic<bool> monitoring_active_{false};
    
    void MetricsCollectionLoop();
    void OptimizeThreadPoolSize();
    void OptimizeMemoryUsage();
    void DetectBottlenecks();
};

// Utilitaires de benchmark
namespace PerformanceBenchmark {
    
    struct BenchmarkResult {
        std::string test_name;
        uint64_t total_frames;
        double duration_seconds;
        double avg_fps;
        double min_fps;
        double max_fps;
        double avg_processing_time_ms;
        uint64_t peak_memory_mb;
        bool success;
        std::string error_message;
    };
    
    // Tests de performance standardisés
    BenchmarkResult BenchmarkMotionDetection(
        const std::string& video_file,
        int num_frames = 1000,
        const PerformanceConfig& config = PerformanceConfig{}
    );
    
    BenchmarkResult BenchmarkThroughput(
        int num_streams,
        int frames_per_stream = 300,
        const PerformanceConfig& config = PerformanceConfig{}
    );
    
    BenchmarkResult BenchmarkMemoryUsage(
        const std::string& video_file,
        int duration_seconds = 60,
        const PerformanceConfig& config = PerformanceConfig{}
    );
    
    // Génération de rapports
    void SaveBenchmarkReport(
        const std::vector<BenchmarkResult>& results, 
        const std::string& filename
    );
    
    void PrintBenchmarkSummary(const std::vector<BenchmarkResult>& results);
    
    // Comparaison de performances
    struct PerformanceComparison {
        std::string baseline_name;
        std::string optimized_name;
        double fps_improvement_percent;
        double memory_reduction_percent;
        double latency_reduction_percent;
        bool significant_improvement;
    };
    
    PerformanceComparison ComparePerformance(
        const BenchmarkResult& baseline,
        const BenchmarkResult& optimized
    );
}

#endif // PERFORMANCE_MANAGER_H

// ============================================================================
// IMPLÉMENTATION SIMPLIFIÉE DES COMPOSANTS CRITIQUES
// ============================================================================

// src/performance_manager.cpp (extrait)

MemoryPool::MemoryPool(size_t pool_size_mb) 
    : pool_size_(pool_size_mb * 1024 * 1024), free_blocks_(nullptr) {
    
    pool_memory_ = static_cast<uint8_t*>(std::aligned_alloc(64, pool_size_));
    if (!pool_memory_) {
        throw std::runtime_error("Failed to allocate memory pool");
    }
    
    // Initialiser le premier bloc libre
    free_blocks_ = reinterpret_cast<Block*>(pool_memory_);
    free_blocks_->ptr = pool_memory_ + sizeof(Block);
    free_blocks_->size = pool_size_ - sizeof(Block);
    free_blocks_->is_free = true;
    free_blocks_->next = nullptr;
    
    std::cerr << "[MemoryPool] Initialized with " << pool_size_mb << "MB" << std::endl;
}

cv::Mat MemoryPool::AllocateMat(int rows, int cols, int type) {
    size_t mat_size = rows * cols * CV_ELEM_SIZE(type);
    
    void* data = Allocate(mat_size, 32); // Alignment 32 bytes pour SIMD
    if (!data) {
        // Fallback vers allocation système
        return cv::Mat(rows, cols, type);
    }
    
    // Créer Mat avec données custom
    return cv::Mat(rows, cols, type, data);
}

VideoProcessingThreadPool::VideoProcessingThreadPool(int num_threads) {
    workers_.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&VideoProcessingThreadPool::WorkerLoop, this);
    }
    
    std::cerr << "[ThreadPool] Started with " << num_threads << " threads" << std::endl;
}

void VideoProcessingThreadPool::WorkerLoop() {
    active_threads_++;
    
    while (!stop_) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return stop_ || !task_queue_.empty(); });
            
            if (stop_ && task_queue_.empty()) {
                break;
            }
            
            task = std::move(task_queue_.front());
            task_queue_.pop();
            queue_size_--;
        }
        
        try {
            task();
            tasks_completed_++;
        } catch (const std::exception& e) {
            std::cerr << "[ThreadPool] Task exception: " << e.what() << std::endl;
        }
    }
    
    active_threads_--;
}

std::future<std::vector<Detection>> VideoProcessingThreadPool::SubmitDetection(
    const cv::Mat& frame, DetectionTask task) {
    
    auto promise = std::make_shared<std::promise<std::vector<Detection>>>();
    auto future = promise->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        task_queue_.emplace([frame = frame.clone(), task, promise]() mutable {
            try {
                auto detections = task(frame);
                promise->set_value(std::move(detections));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
        
        queue_size_++;
    }
    
    condition_.notify_one();
    return future;
}

// Exemple d'utilisation du framework de performance
namespace UsageExample {

void ProcessVideoWithPerformanceOptimization(const std::string& video_file) {
    // Configuration optimisée
    PerformanceConfig config;
    config.max_threads = 8;
    config.enable_memory_pool = true;
    config.memory_pool_size_mb = 1024;
    config.enable_profiling = true;
    config.enable_gpu = true;
    
    // Initialiser le gestionnaire de performance
    PerformanceManager perf_manager(config);
    if (!perf_manager.Initialize()) {
        std::cerr << "Failed to initialize performance manager" << std::endl;
        return;
    }
    
    // Ouvrir la vidéo
    cv::VideoCapture cap(video_file);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video: " << video_file << std::endl;
        return;
    }
    
    // Créer le détecteur de mouvement
    MotionDetectionConfig motion_config;
    motion_config.threshold = 25.0;
    motion_config.min_area = 500;
    
    OpenCVMotionDetector detector(motion_config);
    detector.Initialize();
    
    // Traitement avec optimisations
    cv::Mat frame;
    int frame_count = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (cap.read(frame) && frame_count < 1000) {
        auto processing_start = std::chrono::high_resolution_clock::now();
        
        // Profiler la détection
        PROFILE_SCOPE(perf_manager.GetProfiler(), "motion_detection");
        
        // Convertir en Frame
        Frame video_frame(frame.cols, frame.rows, "bgr");
        video_frame.data.resize(frame.total() * frame.elemSize());
        std::memcpy(video_frame.data.data(), frame.data, video_frame.data.size());
        
        // Détecter le mouvement
        auto detections = detector.Detect(video_frame);
        
        auto processing_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            processing_end - processing_start).count();
        
        // Mettre à jour les métriques
        perf_manager.UpdateMetrics("motion_detection", duration, frame.total() * frame.elemSize());
        
        frame_count++;
        
        if (frame_count % 100 == 0) {
            auto metrics = perf_manager.GetMetrics();
            std::cerr << "Processed " << frame_count << " frames, "
                      << "FPS: " << metrics.current_fps.load() << ", "
                      << "Avg processing: " << metrics.GetAverageProcessingTimeMs() << "ms"
                      << std::endl;
        }
    }
    
    // Rapport final
    auto total_time = std::chrono::high_resolution_clock::now() - start_time;
    auto total_seconds = std::chrono::duration<double>(total_time).count();
    
    std::cerr << "\n=== PERFORMANCE REPORT ===" << std::endl;
    std::cerr << "Total frames: " << frame_count << std::endl;
    std::cerr << "Total time: " << total_seconds << "s" << std::endl;
    std::cerr << "Average FPS: " << frame_count / total_seconds << std::endl;
    
    perf_manager.GetProfiler()->PrintReport();
    
    perf_manager.Shutdown();
}

} // namespace UsageExample
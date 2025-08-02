#ifndef SERVICE_METRICS_H
#define SERVICE_METRICS_H

#include <atomic>
#include <cstdint>

class ServiceMetrics {
public:
    static ServiceMetrics& Instance();
    
    void IncrementStreamsStarted();
    void IncrementFramesProcessed();
    void IncrementDetections();
    void RecordProcessingTime(int64_t time_ms);
    
    int64_t GetStreamsStarted() const;
    int64_t GetFramesProcessed() const;
    int64_t GetDetections() const;
    double GetAverageProcessingTime() const;
    
private:
    ServiceMetrics() = default;
    
    std::atomic<int64_t> streams_started_{0};
    std::atomic<int64_t> frames_processed_{0};
    std::atomic<int64_t> detections_{0};
    std::atomic<int64_t> total_processing_time_{0};
    std::atomic<int64_t> processing_samples_{0};
};

#endif // SERVICE_METRICS_H
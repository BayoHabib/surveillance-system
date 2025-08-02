#include "service_metrics.h"

ServiceMetrics& ServiceMetrics::Instance() {
    static ServiceMetrics instance;
    return instance;
}

void ServiceMetrics::IncrementStreamsStarted() {
    streams_started_++;
}

void ServiceMetrics::IncrementFramesProcessed() {
    frames_processed_++;
}

void ServiceMetrics::IncrementDetections() {
    detections_++;
}

void ServiceMetrics::RecordProcessingTime(int64_t time_ms) {
    total_processing_time_ += time_ms;
    processing_samples_++;
}

int64_t ServiceMetrics::GetStreamsStarted() const {
    return streams_started_.load();
}

int64_t ServiceMetrics::GetFramesProcessed() const {
    return frames_processed_.load();
}

int64_t ServiceMetrics::GetDetections() const {
    return detections_.load();
}

double ServiceMetrics::GetAverageProcessingTime() const {
    int64_t samples = processing_samples_.load();
    if (samples == 0) return 0.0;
    
    return static_cast<double>(total_processing_time_.load()) / samples;
}
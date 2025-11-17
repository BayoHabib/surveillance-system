// src/opencv_motion_detector.cpp
// Détecteur de mouvement avancé avec OpenCV

#include "opencv_motion_detector.h"
#include <iostream>
#include <sstream>
#include <chrono>

using namespace cv;

OpenCVMotionDetector::OpenCVMotionDetector(const MotionDetectionConfig& config)
    : config_(config), initialized_(false), debug_output_(false) {
    std::cerr << "[OpenCVMotionDetector] Constructed with threshold: " << config_.threshold << std::endl;
}

OpenCVMotionDetector::~OpenCVMotionDetector() {
    Cleanup();
}

bool OpenCVMotionDetector::Initialize() {
    std::cerr << "[OpenCVMotionDetector] Initializing with OpenCV BackgroundSubtractorMOG2" << std::endl;
    
    try {
        // Créer le background subtractor MOG2
        bg_subtractor_ = createBackgroundSubtractorMOG2(
            config_.history,                    // History frames
            config_.threshold,                  // Threshold
            config_.enable_shadow_detection     // Detect shadows
        );
        
        if (!bg_subtractor_) {
            std::cerr << "[OpenCVMotionDetector] Failed to create BackgroundSubtractorMOG2" << std::endl;
            return false;
        }

        // Configurer les paramètres avancés
        bg_subtractor_->setVarThreshold(config_.threshold);
        bg_subtractor_->setDetectShadows(config_.enable_shadow_detection);
        bg_subtractor_->setHistory(config_.history);
        bg_subtractor_->setVarInit(15.0);              // Variance initiale
        bg_subtractor_->setVarMin(4.0);                // Variance minimale
        bg_subtractor_->setVarMax(75.0);               // Variance maximale
        bg_subtractor_->setComplexityReductionThreshold(0.05); // Réduction complexité
        bg_subtractor_->setBackgroundRatio(0.9);       // Ratio background
        bg_subtractor_->setNMixtures(5);               // Nombre de gaussiennes
        bg_subtractor_->setShadowThreshold(0.5);       // Seuil ombres
        bg_subtractor_->setShadowValue(127);           // Valeur ombres

        // Créer l'élément morphologique pour le post-traitement
        if (config_.enable_noise_reduction && config_.morphology_size > 0) {
            morphology_kernel_ = getStructuringElement(
                MORPH_ELLIPSE, 
                Size(config_.morphology_size, config_.morphology_size)
            );
        }

        initialized_ = true;
        total_detections_ = 0;
        false_positives_filtered_ = 0;

        std::cerr << "[OpenCVMotionDetector] Initialization successful" << std::endl;
        std::cerr << "[OpenCVMotionDetector] Config: threshold=" << config_.threshold 
                  << ", min_area=" << config_.min_area 
                  << ", max_area=" << config_.max_area 
                  << ", learning_rate=" << config_.learning_rate << std::endl;

        return true;

    } catch (const Exception& e) {
        std::cerr << "[OpenCVMotionDetector] OpenCV exception during initialization: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[OpenCVMotionDetector] Exception during initialization: " << e.what() << std::endl;
        return false;
    }
}

void OpenCVMotionDetector::Cleanup() {
    std::cerr << "[OpenCVMotionDetector] Cleanup" << std::endl;
    
    if (bg_subtractor_) {
        bg_subtractor_.release();
    }
    
    foreground_mask_.release();
    background_image_.release();
    previous_frame_.release();
    morphology_kernel_.release();
    
    initialized_ = false;
}

std::vector<Detection> OpenCVMotionDetector::Detect(const Frame& frame) {
    std::vector<Detection> detections;
    
    if (!initialized_) {
        std::cerr << "[OpenCVMotionDetector] Not initialized" << std::endl;
        return detections;
    }

    try {
        // Convertir Frame vers Mat
        Mat current_frame;
        if (frame.format == "bgr") {
            current_frame = Mat(frame.height, frame.width, CV_8UC3, (void*)frame.data.data());
        } else if (frame.format == "gray") {
            current_frame = Mat(frame.height, frame.width, CV_8UC1, (void*)frame.data.data());
        } else {
            std::cerr << "[OpenCVMotionDetector] Unsupported frame format: " << frame.format << std::endl;
            return detections;
        }

        if (current_frame.empty()) {
            std::cerr << "[OpenCVMotionDetector] Empty frame received" << std::endl;
            return detections;
        }

        // Convertir en niveaux de gris si nécessaire
        Mat gray_frame;
        if (current_frame.channels() == 3) {
            cvtColor(current_frame, gray_frame, COLOR_BGR2GRAY);
        } else {
            gray_frame = current_frame.clone();
        }

        // Appliquer le background subtractor
        bg_subtractor_->apply(gray_frame, foreground_mask_, config_.learning_rate);

        if (foreground_mask_.empty()) {
            std::cerr << "[OpenCVMotionDetector] Empty foreground mask" << std::endl;
            return detections;
        }

        // Post-traitement pour réduire le bruit
        if (config_.enable_noise_reduction) {
            ApplyNoiseReduction(foreground_mask_);
        }

        // Obtenir l'image de background pour debug
        if (debug_output_) {
            bg_subtractor_->getBackgroundImage(background_image_);
        }

        // Traiter le masque pour extraire les détections
        int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        detections = ProcessForegroundMask(foreground_mask_, timestamp);
        
        // Mettre à jour les statistiques
        total_detections_ += detections.size();
        
        // Stocker la frame précédente pour analyse supplémentaire si nécessaire
        previous_frame_ = gray_frame.clone();

        if (debug_output_ && !detections.empty()) {
            std::cerr << "[OpenCVMotionDetector] Detected " << detections.size() << " objects" << std::endl;
        }

    } catch (const Exception& e) {
        std::cerr << "[OpenCVMotionDetector] OpenCV exception during detection: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[OpenCVMotionDetector] Exception during detection: " << e.what() << std::endl;
    }

    return detections;
}

std::vector<Detection> OpenCVMotionDetector::ProcessForegroundMask(const Mat& mask, int64_t timestamp) {
    std::vector<Detection> detections;

    // Trouver les contours
    std::vector<std::vector<Point>> contours;
    std::vector<Vec4i> hierarchy;
    
    findContours(mask, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        // Calculer l'aire du contour
        double area = contourArea(contour);
        
        // Filtrer par aire
        if (!FilterDetection(Rect(), area)) {
            false_positives_filtered_++;
            continue;
        }

        // Obtenir le rectangle englobant
        Rect bounding_rect = boundingRect(contour);
        
        // Filtrer par dimensions du rectangle
        if (!FilterDetection(bounding_rect, area)) {
            false_positives_filtered_++;
            continue;
        }

        // Créer la détection
        Detection detection = CreateDetectionFromContour(contour, timestamp);
        
        // Remplir le bounding box
        auto* bbox = detection.mutable_bbox();
        bbox->set_x(bounding_rect.x);
        bbox->set_y(bounding_rect.y);
        bbox->set_width(bounding_rect.width);
        bbox->set_height(bounding_rect.height);

        // Calculer la confiance basée sur l'aire et la forme
        float confidence = CalculateConfidence(contour, area, bounding_rect);
        detection.set_confidence(confidence);

        detections.push_back(detection);

        if (debug_output_) {
            std::cerr << "[OpenCVMotionDetector] Detection: area=" << area 
                      << ", bbox=" << bounding_rect.x << "," << bounding_rect.y 
                      << "," << bounding_rect.width << "," << bounding_rect.height 
                      << ", confidence=" << confidence << std::endl;
        }
    }

    return detections;
}

bool OpenCVMotionDetector::FilterDetection(const Rect& bbox, double area) const {
    // Filtrer par aire
    if (area < config_.min_area || area > config_.max_area) {
        return false;
    }

    // Filtrer par ratio aspect si bbox disponible
    if (bbox.width > 0 && bbox.height > 0) {
        double aspect_ratio = static_cast<double>(bbox.width) / bbox.height;
        
        // Rejeter les objets trop étirés (probablement du bruit)
        if (aspect_ratio > 10.0 || aspect_ratio < 0.1) {
            return false;
        }

        // Vérifier la densité de l'objet
        double density = area / (bbox.width * bbox.height);
        if (density < 0.3) { // Objet trop creux
            return false;
        }
    }

    return true;
}

Detection OpenCVMotionDetector::CreateDetectionFromContour(const std::vector<Point>& contour, int64_t timestamp) const {
    Detection detection;
    
    detection.set_id(GenerateDetectionId());
    detection.set_type("motion");
    detection.set_timestamp(timestamp);

    // Calculer des propriétés géométriques
    Moments moments = cv::moments(contour);
    
    // Centre de masse
    if (moments.m00 > 0) {
        int center_x = static_cast<int>(moments.m10 / moments.m00);
        int center_y = static_cast<int>(moments.m01 / moments.m00);
        
        // Ajouter aux métadonnées
        auto& metadata = *detection.mutable_metadata();
        metadata["center_x"] = std::to_string(center_x);
        metadata["center_y"] = std::to_string(center_y);
        metadata["contour_area"] = std::to_string(contourArea(contour));
        metadata["perimeter"] = std::to_string(arcLength(contour, true));
        metadata["detector"] = "OpenCVMotionDetector";
        metadata["algorithm"] = "BackgroundSubtractorMOG2";
    }

    return detection;
}

float OpenCVMotionDetector::CalculateConfidence(const std::vector<Point>& contour, 
                                                double area, 
                                                const Rect& bbox) const {
    // Confiance basée sur plusieurs facteurs
    float confidence = 0.5f; // Confiance de base

    // Facteur 1: Taille de l'objet (plus c'est gros, plus c'est probable)
    double normalized_area = std::min(area / config_.max_area, 1.0);
    confidence += static_cast<float>(normalized_area * 0.2);

    // Facteur 2: Compacité de l'objet
    double perimeter = arcLength(contour, true);
    if (perimeter > 0) {
        double compactness = 4 * M_PI * area / (perimeter * perimeter);
        confidence += static_cast<float>(compactness * 0.2);
    }

    // Facteur 3: Solidité (rapport aire/convex hull)
    std::vector<Point> hull;
    convexHull(contour, hull);
    double hull_area = contourArea(hull);
    if (hull_area > 0) {
        double solidity = area / hull_area;
        confidence += static_cast<float>(solidity * 0.1);
    }

    // Limiter entre 0 et 1
    return std::max(0.0f, std::min(1.0f, confidence));
}

void OpenCVMotionDetector::ApplyNoiseReduction(Mat& mask) const {
    if (morphology_kernel_.empty()) {
        return;
    }

    // Opérations morphologiques pour réduire le bruit
    
    // 1. Opening (erosion suivie de dilatation) pour éliminer le bruit
    morphologyEx(mask, mask, MORPH_OPEN, morphology_kernel_);
    
    // 2. Closing (dilatation suivie d'érosion) pour fermer les trous
    morphologyEx(mask, mask, MORPH_CLOSE, morphology_kernel_);
    
    // 3. Filtrage médian pour lisser
    medianBlur(mask, mask, 3);
}

std::string OpenCVMotionDetector::GenerateDetectionId() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();
    
    std::stringstream ss;
    ss << "opencv_motion_" << timestamp << "_" << total_detections_.load();
    return ss.str();
}

void OpenCVMotionDetector::SetConfig(const MotionDetectionConfig& config) {
    config_ = config;
    
    if (initialized_ && bg_subtractor_) {
        // Mettre à jour les paramètres du background subtractor
        bg_subtractor_->setVarThreshold(config_.threshold);
        bg_subtractor_->setDetectShadows(config_.enable_shadow_detection);
        bg_subtractor_->setHistory(config_.history);
        
        // Recréer l'élément morphologique si nécessaire
        if (config_.enable_noise_reduction && config_.morphology_size > 0) {
            morphology_kernel_ = getStructuringElement(
                MORPH_ELLIPSE, 
                Size(config_.morphology_size, config_.morphology_size)
            );
        }
        
        std::cerr << "[OpenCVMotionDetector] Configuration updated" << std::endl;
    }
}

MotionDetectionConfig OpenCVMotionDetector::GetConfig() const {
    return config_;
}

Mat OpenCVMotionDetector::GetLastForegroundMask() const {
    return foreground_mask_.clone();
}

Mat OpenCVMotionDetector::GetBackgroundImage() const {
    return background_image_.clone();
}

// Fonctions utilitaires pour le debug et la visualisation
namespace OpenCVMotionDetectorUtils {

void DrawDetections(Mat& image, const std::vector<Detection>& detections, const Scalar& color = Scalar(0, 255, 0)) {
    for (const auto& detection : detections) {
        const auto& bbox = detection.bbox();
        
        // Dessiner le rectangle
        rectangle(image, 
                 Point(bbox.x(), bbox.y()), 
                 Point(bbox.x() + bbox.width(), bbox.y() + bbox.height()), 
                 color, 2);
        
        // Ajouter le texte de confiance
        std::string confidence_text = "conf: " + std::to_string(detection.confidence()).substr(0, 4);
        putText(image, confidence_text, 
                Point(bbox.x(), bbox.y() - 10), 
                FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    }
}

void SaveDebugImage(const Mat& image, const std::string& filename) {
    try {
        imwrite(filename, image);
        std::cerr << "[OpenCVMotionDetector] Debug image saved: " << filename << std::endl;
    } catch (const Exception& e) {
        std::cerr << "[OpenCVMotionDetector] Failed to save debug image: " << e.what() << std::endl;
    }
}

Mat CreateDebugVisualization(const Mat& original, 
                            const Mat& foreground_mask, 
                            const Mat& background,
                            const std::vector<Detection>& detections) {
    Mat visualization;
    
    // Créer une visualisation 2x2
    Mat top_row, bottom_row;
    
    // Ligne du haut: original et foreground mask
    Mat original_bgr = original.clone();
    if (original.channels() == 1) {
        cvtColor(original, original_bgr, COLOR_GRAY2BGR);
    }
    
    Mat mask_bgr;
    cvtColor(foreground_mask, mask_bgr, COLOR_GRAY2BGR);
    
    hconcat(original_bgr, mask_bgr, top_row);
    
    // Ligne du bas: background et détections
    Mat background_bgr = background.clone();
    if (background.channels() == 1) {
        cvtColor(background, background_bgr, COLOR_GRAY2BGR);
    }
    
    Mat detections_img = original_bgr.clone();
    DrawDetections(detections_img, detections);
    
    hconcat(background_bgr, detections_img, bottom_row);
    
    // Combiner les deux lignes
    vconcat(top_row, bottom_row, visualization);
    
    // Ajouter des labels
    putText(visualization, "Original", Point(10, 30), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
    putText(visualization, "Foreground", Point(original.cols + 10, 30), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
    putText(visualization, "Background", Point(10, original.rows + 30), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
    putText(visualization, "Detections", Point(original.cols + 10, original.rows + 30), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
    
    return visualization;
}

} // namespace OpenCVMotionDetectorUtils
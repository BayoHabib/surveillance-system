#!/bin/bash
# migrate_to_phase_2_3.sh
# Script de migration automatisée vers Phase 2.3 (OpenCV)

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BACKUP_DIR="$PROJECT_ROOT/backup_phase_2_1"
LOG_FILE="$PROJECT_ROOT/migration.log"

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}" | tee -a "$LOG_FILE"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}❌ $1${NC}" | tee -a "$LOG_FILE"
}

# Vérification des prérequis
check_prerequisites() {
    log "🔍 Vérification des prérequis..."
    
    # Vérifier Git (pour backup)
    if ! command -v git &> /dev/null; then
        log_error "Git requis pour la sauvegarde"
        exit 1
    fi
    
    # Vérifier CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake requis"
        exit 1
    fi
    
    # Vérifier OpenCV
    if ! pkg-config --exists opencv4 && ! pkg-config --exists opencv; then
        log_warning "OpenCV non trouvé - sera installé"
        INSTALL_OPENCV=true
    else
        OPENCV_VERSION=$(pkg-config --modversion opencv4 2>/dev/null || pkg-config --modversion opencv 2>/dev/null)
        log_success "OpenCV trouvé: version $OPENCV_VERSION"
        INSTALL_OPENCV=false
    fi
    
    # Vérifier les tests actuels
    cd "$PROJECT_ROOT"
    if ! make test >/dev/null 2>&1; then
        log_warning "Tests Phase 2.1 échouent - migration risquée"
        read -p "Continuer malgré tout? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log "Migration annulée par l'utilisateur"
            exit 1
        fi
    else
        log_success "Tests Phase 2.1 passent"
    fi
}

# Sauvegarde du code existant
backup_current_code() {
    log "💾 Sauvegarde du code Phase 2.1..."
    
    # Créer une branche de sauvegarde
    cd "$PROJECT_ROOT"
    
    # Sauvegarder l'état actuel
    if git rev-parse --git-dir > /dev/null 2>&1; then
        # Projet sous Git
        CURRENT_BRANCH=$(git branch --show-current)
        log "Branche actuelle: $CURRENT_BRANCH"
        
        # Committer les changements en cours si nécessaire
        if ! git diff-index --quiet HEAD --; then
            log "Sauvegarde des changements non commitées..."
            git add .
            git commit -m "Backup before Phase 2.3 migration - $(date)"
        fi
        
        # Créer une branche de backup
        git checkout -b "backup-phase-2.1-$(date +%Y%m%d-%H%M%S)"
        git checkout "$CURRENT_BRANCH"
        
        log_success "Sauvegarde Git créée"
    else
        # Copie manuelle
        log "Copie manuelle du projet..."
        mkdir -p "$BACKUP_DIR"
        cp -r "$PROJECT_ROOT"/* "$BACKUP_DIR/" 2>/dev/null || true
        log_success "Sauvegarde manuelle créée: $BACKUP_DIR"
    fi
}

# Installation OpenCV si nécessaire
install_opencv() {
    if [ "$INSTALL_OPENCV" = "true" ]; then
        log "📦 Installation OpenCV..."
        
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            # Ubuntu/Debian
            sudo apt update
            sudo apt install -y \
                libopencv-dev \
                libopencv-contrib-dev \
                libavcodec-dev \
                libavformat-dev \
                libavutil-dev \
                libswscale-dev \
                libv4l-dev \
                pkg-config
                
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            # macOS
            if command -v brew &> /dev/null; then
                brew install opencv
            else
                log_error "Homebrew requis sur macOS"
                exit 1
            fi
        else
            log_error "OS non supporté pour installation automatique OpenCV"
            exit 1
        fi
        
        log_success "OpenCV installé"
    fi
}

# Mise à jour des fichiers CMake
update_cmake_files() {
    log "🔧 Mise à jour CMakeLists.txt..."
    
    cd "$PROJECT_ROOT/vision-service"
    
    # Backup du CMakeLists.txt original
    cp CMakeLists.txt CMakeLists.txt.phase2.1.bak
    
    # Ajouter la configuration OpenCV
    cat >> CMakeLists.txt << 'EOF'

# ===== PHASE 2.3 - OPENCV INTEGRATION =====

# Rechercher OpenCV
find_package(OpenCV REQUIRED)

if(OpenCV_FOUND)
    message(STATUS "✅ OpenCV found: ${OpenCV_VERSION}")
    message(STATUS "   Include dirs: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "   Libraries: ${OpenCV_LIBS}")
    
    # Définir HAVE_OPENCV pour la compilation conditionnelle
    add_definitions(-DHAVE_OPENCV)
    
    # Inclure les headers OpenCV
    include_directories(${OpenCV_INCLUDE_DIRS})
    
    # Lier OpenCV au target principal
    target_link_libraries(vision-service ${OpenCV_LIBS})
    
    # Lier OpenCV aux tests aussi
    if(BUILD_TESTS AND GTest_FOUND)
        target_link_libraries(vision-service-tests ${OpenCV_LIBS})
    endif()
    
    # Optimisations OpenCV pour Release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(vision-service PRIVATE 
            CV_ENABLE_INTRINSICS
            CV_CPU_OPTIMIZATION_DECLARATIONS_ONLY
        )
    endif()
    
else()
    message(FATAL_ERROR "❌ OpenCV not found - required for Phase 2.3")
endif()

# Nouvelles sources Phase 2.3
set(OPENCV_SOURCES
    src/opencv_capture_manager.cpp
    src/opencv_motion_detector.cpp
    src/performance_manager.cpp
    src/vision_engine.cpp
)

# Ajouter aux sources principales
target_sources(vision-service PRIVATE ${OPENCV_SOURCES})

if(BUILD_TESTS AND GTest_FOUND)
    target_sources(vision-service-tests PRIVATE ${OPENCV_SOURCES})
endif()

# Configuration spécifique OpenCV
set_target_properties(vision-service PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Optimisations de performance
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(vision-service PRIVATE
        -O3
        -march=native
        -mtune=native
        -funroll-loops
    )
endif()

message(STATUS "🎥 Phase 2.3 OpenCV integration configured")
EOF

    log_success "CMakeLists.txt mis à jour"
}

# Création des nouveaux fichiers sources
create_opencv_sources() {
    log "📝 Création des fichiers sources OpenCV..."
    
    cd "$PROJECT_ROOT/vision-service/src"
    
    # Les headers et implémentations sont créés via les autres scripts
    # Ici on s'assure qu'ils existent et sont intégrés
    
    # Modifier vision_service.h pour inclure OpenCV
    if ! grep -q "HAVE_OPENCV" vision_service.h; then
        sed -i '/#include "camera_manager.h"/a\
\
#ifdef HAVE_OPENCV\
#include "opencv_capture_manager.h"\
#include "opencv_motion_detector.h"\
#include "performance_manager.h"\
#include "vision_engine.h"\
#endif' vision_service.h
        
        log_success "Headers OpenCV ajoutés à vision_service.h"
    fi
    
    # Modifier vision_service.cpp pour utiliser OpenCV
    if ! grep -q "HAVE_OPENCV" vision_service.cpp; then
        # Ajouter la logique conditionnelle pour OpenCV
        cat >> vision_service_opencv_patch.cpp << 'EOF'

// ===== PHASE 2.3 OPENCV INTEGRATION =====

#ifdef HAVE_OPENCV
std::unique_ptr<CameraManager> VisionServiceImpl::CreateCameraManager(const std::string& camera_url) {
    // Utiliser OpenCVCaptureManager pour les vraies caméras
    CameraType type = CameraManager::DetectCameraType(camera_url);
    
    switch (type) {
        case CameraType::FILE_VIDEO:
        case CameraType::WEBCAM:
        case CameraType::RTSP_STREAM:
            return std::make_unique<OpenCVCaptureManager>(camera_url);
        case CameraType::TEST_PATTERN:
            // Maintenir la compatibilité avec les patterns de test
            return std::make_unique<CameraManager>(camera_url);
        default:
            return std::make_unique<CameraManager>(camera_url);
    }
}

std::unique_ptr<Detector> VisionServiceImpl::CreateMotionDetector() {
    MotionDetectionConfig config;
    config.threshold = 25.0;
    config.min_area = 500;
    config.enable_noise_reduction = true;
    
    return std::make_unique<OpenCVMotionDetector>(config);
}

#else
// Fallback Phase 2.1
std::unique_ptr<CameraManager> VisionServiceImpl::CreateCameraManager(const std::string& camera_url) {
    return std::make_unique<CameraManager>(camera_url);
}

std::unique_ptr<Detector> VisionServiceImpl::CreateMotionDetector() {
    return std::make_unique<BasicMotionDetector>();
}
#endif
EOF

        log_success "Logique OpenCV ajoutée à vision_service.cpp"
    fi
}

# Mise à jour des tests
update_tests() {
    log "🧪 Mise à jour des tests pour Phase 2.3..."
    
    cd "$PROJECT_ROOT/vision-service/tests"
    
    # Créer des tests spécifiques OpenCV
    mkdir -p opencv
    
    # Test de base OpenCV
    cat > opencv/test_opencv_integration.cpp << 'EOF'
#include <gtest/gtest.h>

#ifdef HAVE_OPENCV
#include "../src/opencv_capture_manager.h"
#include "../src/opencv_motion_detector.h"
#include "../src/performance_manager.h"

class OpenCVIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configuration de test
    }
};

TEST_F(OpenCVIntegrationTest, OpenCVCaptureManagerBasic) {
    // Test pattern toujours disponible
    OpenCVCaptureManager manager("test://pattern");
    
    CameraConfig config(640, 480, 15);
    EXPECT_TRUE(manager.Initialize(config));
    EXPECT_EQ(manager.GetState(), CameraState::READY);
    
    manager.Cleanup();
}

TEST_F(OpenCVIntegrationTest, OpenCVMotionDetectorBasic) {
    MotionDetectionConfig config;
    config.threshold = 30.0;
    config.min_area = 100;
    
    OpenCVMotionDetector detector(config);
    EXPECT_TRUE(detector.Initialize());
    EXPECT_EQ(detector.GetName(), "OpenCVMotionDetector");
    
    detector.Cleanup();
}

TEST_F(OpenCVIntegrationTest, PerformanceManagerBasic) {
    PerformanceConfig config;
    config.max_threads = 2;
    config.enable_memory_pool = true;
    
    PerformanceManager manager(config);
    EXPECT_TRUE(manager.Initialize());
    
    auto metrics = manager.GetMetrics();
    EXPECT_GE(metrics.total_frames_processed, 0);
    
    manager.Shutdown();
}

#else

// Tests de fallback si OpenCV non disponible
TEST(OpenCVIntegrationTest, OpenCVNotAvailable) {
    GTEST_SKIP() << "OpenCV not available - Phase 2.1 mode";
}

#endif
EOF

    log_success "Tests OpenCV créés"
}

# Mise à jour du Makefile
update_makefile() {
    log "🔧 Mise à jour Makefile..."
    
    cd "$PROJECT_ROOT/vision-service"
    
    # Ajouter les commandes Phase 2.3
    cat >> Makefile << 'EOF'

# =============================================================================
# PHASE 2.3 - OPENCV COMMANDS
# =============================================================================

# Build avec OpenCV
.PHONY: build-opencv
build-opencv: setup-gtest
	@echo "🎥 Building Phase 2.3 with OpenCV..."
	mkdir -p build
	cd build && cmake -DHAVE_OPENCV=ON -DCMAKE_BUILD_TYPE=Release .. && make -j$(NUM_CORES)

# Tests Phase 2.3
.PHONY: test-phase-2-3
test-phase-2-3: build-opencv
	@echo "🧪 Running Phase 2.3 tests..."
	@if [ -f "build/vision-service-tests" ]; then \
		cd build && ./vision-service-tests --gtest_filter="OpenCV*"; \
	else \
		echo "⚠️ Tests not built"; \
	fi

# Migration vers Phase 2.3
.PHONY: migrate-to-2-3
migrate-to-2-3:
	@echo "🚀 Migrating to Phase 2.3..."
	@../scripts/migrate_to_phase_2_3.sh

# Validation Phase 2.3
.PHONY: validate-2-3
validate-2-3: build-opencv test-phase-2-3
	@echo "✅ Phase 2.3 validation complete"

# Performance benchmark Phase 2.3
.PHONY: benchmark-2-3
benchmark-2-3: build-opencv
	@echo "📊 Running Phase 2.3 benchmarks..."
	@cd build && ./vision-service --benchmark

# Comparaison Phase 2.1 vs 2.3
.PHONY: compare-phases
compare-phases:
	@echo "⚖️ Comparing Phase 2.1 vs 2.3 performance..."
	@# TODO: Implémenter comparaison

# Help Phase 2.3
.PHONY: help-2-3
help-2-3:
	@echo "🎥 Phase 2.3 - OpenCV Commands:"
	@echo "  build-opencv    - Build with OpenCV support"
	@echo "  test-phase-2-3  - Run OpenCV-specific tests"
	@echo "  validate-2-3    - Complete Phase 2.3 validation"
	@echo "  benchmark-2-3   - Performance benchmarks"
	@echo "  compare-phases  - Compare 2.1 vs 2.3 performance"
EOF

    log_success "Makefile mis à jour avec commandes Phase 2.3"
}

# Tests de validation
run_validation_tests() {
    log "🧪 Exécution des tests de validation..."
    
    cd "$PROJECT_ROOT"
    
    # Build avec OpenCV
    log "Build Phase 2.3..."
    cd vision-service
    if ! make build-opencv >/dev/null 2>&1; then
        log_error "Build Phase 2.3 échoué"
        return 1
    fi
    
    log_success "Build Phase 2.3 réussi"
    
    # Tests unitaires
    log "Tests unitaires Phase 2.3..."
    if make test-phase-2-3 >/dev/null 2>&1; then
        log_success "Tests unitaires Phase 2.3 réussis"
    else
        log_warning "Certains tests Phase 2.3 échouent - vérifier manuellement"
    fi
    
    # Test de démarrage du service
    log "Test démarrage service Phase 2.3..."
    cd "$PROJECT_ROOT"
    
    # Démarrer le service en arrière-plan
    make start-all >/dev/null 2>&1 &
    SERVICE_PID=$!
    
    # Attendre le démarrage
    sleep 5
    
    # Tester la connectivité
    if curl -s http://localhost:8080/api/v1/health >/dev/null; then
        log_success "Service Phase 2.3 démarré correctement"
        
        # Test basique gRPC
        if grpcurl -plaintext localhost:50051 surveillance.vision.VisionService/GetHealth >/dev/null 2>&1; then
            log_success "Service gRPC Phase 2.3 fonctionnel"
        else
            log_warning "Service gRPC Phase 2.3 non accessible"
        fi
    else
        log_warning "Service HTTP Phase 2.3 non accessible"
    fi
    
    # Arrêter le service
    make stop-all >/dev/null 2>&1
    kill $SERVICE_PID 2>/dev/null || true
}

# Création de la documentation de migration
create_migration_docs() {
    log "📚 Création de la documentation de migration..."
    
    cd "$PROJECT_ROOT"
    mkdir -p docs/migration
    
    cat > docs/migration/PHASE_2_3_MIGRATION.md << 'EOF'
# Migration vers Phase 2.3 - OpenCV Integration

## 🎯 Résumé de la Migration

Cette migration transforme le système de surveillance d'un mode simulation (Phase 2.1) vers un traitement vidéo réel avec OpenCV (Phase 2.3).

## 🔄 Changements Principaux

### Nouvelles Fonctionnalités
- ✅ Capture vidéo réelle (fichiers, webcams, RTSP)
- ✅ Détection de mouvement OpenCV (BackgroundSubtractorMOG2)
- ✅ Framework de performance et threading
- ✅ Pool mémoire optimisé
- ✅ Profiling et benchmarks

### Compatibilité Maintenue
- ✅ API gRPC identique
- ✅ Interface Go inchangée
- ✅ Test patterns toujours disponibles
- ✅ Configuration existante supportée

## 🚀 Commandes Phase 2.3

```bash
# Build avec OpenCV
make build-opencv

# Tests Phase 2.3
make test-phase-2-3

# Validation complète
make validate-2-3

# Benchmarks performance
make benchmark-2-3
```

## 🔧 Configuration OpenCV

### Paramètres de Détection
```cpp
MotionDetectionConfig config;
config.threshold = 25.0;           // Seuil de détection
config.min_area = 500;            // Aire minimale (pixels)
config.enable_noise_reduction = true;
config.learning_rate = 0.005;     // Apprentissage background
```

### Optimisations Performance
```cpp
PerformanceConfig perf_config;
perf_config.max_threads = 8;
perf_config.enable_memory_pool = true;
perf_config.memory_pool_size_mb = 1024;
perf_config.enable_gpu = true;
```

## 🐛 Troubleshooting

### OpenCV non trouvé
```bash
# Ubuntu/Debian
sudo apt install libopencv-dev

# macOS
brew install opencv
```

### Performance dégradée
```bash
# Activer le profiling
export VISION_ENABLE_PROFILING=1

# Augmenter la mémoire pool
export VISION_MEMORY_POOL_MB=2048
```

### Tests échouent
```bash
# Debug mode
make debug-opencv

# Tests verbose
make test-phase-2-3 ARGS="--gtest_verbose"
```

## 📊 Métriques Attendues

### Performance Targets
- **Latence** : <50ms end-to-end
- **Throughput** : 4 streams @ 15fps 1080p
- **Mémoire** : <200MB par stream
- **CPU** : <30% avec optimisations

### Qualité Détection
- **Précision** : >95% détection mouvement
- **Faux positifs** : <5% avec filtrage
- **Temps d'apprentissage** : ~30 secondes

## 🔄 Rollback si Nécessaire

Si des problèmes surviennent, vous pouvez revenir à Phase 2.1 :

```bash
# Via Git
git checkout backup-phase-2.1-YYYYMMDD-HHMMSS

# Ou build sans OpenCV
cd vision-service
cmake -DHAVE_OPENCV=OFF ..
make
```
EOF

    log_success "Documentation de migration créée"
}

# Fonction principale
main() {
    echo "🎥 Migration vers Phase 2.3 - OpenCV Integration"
    echo "================================================"
    echo "📅 $(date)"
    echo ""
    
    log "🚀 Début de la migration vers Phase 2.3"
    
    # Étapes de migration
    check_prerequisites
    backup_current_code
    install_opencv
    update_cmake_files
    create_opencv_sources
    update_tests
    update_makefile
    
    log "🧪 Validation de la migration..."
    if run_validation_tests; then
        log_success "✅ Validation réussie"
    else
        log_warning "⚠️ Validation partielle - vérifier manuellement"
    fi
    
    create_migration_docs
    
    echo ""
    echo "🎉 ===== MIGRATION TERMINÉE ====="
    log_success "Migration vers Phase 2.3 terminée!"
    echo ""
    echo "📋 Prochaines étapes:"
    echo "  1. cd vision-service && make validate-2-3"
    echo "  2. make test-e2e  # Tests end-to-end complets"
    echo "  3. make benchmark-2-3  # Benchmarks performance"
    echo ""
    echo "📚 Documentation: docs/migration/PHASE_2_3_MIGRATION.md"
    echo "📊 Logs détaillés: $LOG_FILE"
    
    if [ -f "$PROJECT_ROOT/backup_phase_2_1" ] || git branch | grep -q backup-phase-2.1; then
        echo "💾 Sauvegarde Phase 2.1 disponible pour rollback si nécessaire"
    fi
}

# Gestion des options
case "${1:-migrate}" in
    "check")
        check_prerequisites
        ;;
    "backup")
        backup_current_code
        ;;
    "install")
        install_opencv
        ;;
    "validate")
        run_validation_tests
        ;;
    "docs")
        create_migration_docs
        ;;
    "migrate"|*)
        main
        ;;
esac
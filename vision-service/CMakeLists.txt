cmake_minimum_required(VERSION 3.16)
project(VisionService VERSION 1.0.0 LANGUAGES CXX)

# Configuration C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type par défaut
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options de compilation
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Trouver les dépendances
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# RE2 (required for regex)
pkg_check_modules(RE2 REQUIRED re2)

# Optionnel : OpenCV (pour Phase 2.3)
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
    add_definitions(-DHAVE_OPENCV)
else()
    message(STATUS "OpenCV not found - will be added in Phase 2.3")
endif()

# Dossiers d'include
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Génération des fichiers protobuf et gRPC
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILES "${PROTO_PATH}/vision.proto")

# Fichiers générés
set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/vision.pb.cc")
set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/vision.pb.h")
set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/vision.grpc.pb.cc")
set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/vision.grpc.pb.h")

# Commandes de génération protobuf
add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         --proto_path=${PROTO_PATH}
         ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Generating protobuf C++ files"
)

# Commandes de génération gRPC
add_custom_command(
    OUTPUT ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
         --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`
         --proto_path=${PROTO_PATH}
         ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
    COMMENT "Generating gRPC C++ files"
)

# Sources du service principal - ADD service_metrics.cpp
set(VISION_SOURCES
    src/main.cpp
    src/vision_service.cpp
    src/service_metrics.cpp
    src/frame_processor.cpp
    src/camera_manager.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# Add OpenCV-specific sources conditionally
if(OpenCV_FOUND)
    list(APPEND VISION_SOURCES
        src/opencv_capture_manager.cpp
        src/opencv_motion_detector.cpp
        src/performance_manager.cpp
    )
endif()

# Headers du service
set(VISION_HEADERS
    src/vision_service.h
    src/service_metrics.h
    src/frame_processor.h
    src/camera_manager.h
    ${PROTO_HDRS}
    ${GRPC_HDRS}
)

# Add OpenCV-specific headers conditionally
if(OpenCV_FOUND)
    list(APPEND VISION_HEADERS
        src/opencv_capture_manager.h
        src/performance_manager.h
    )
endif()

# Exécutable principal
add_executable(vision-service ${VISION_SOURCES} ${VISION_HEADERS})

# Bibliothèques à lier - ADD grpc++_reflection and re2
target_link_libraries(vision-service
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    ${RE2_LIBRARIES}
    pthread
)

# Include directories for RE2
target_include_directories(vision-service PRIVATE ${RE2_INCLUDE_DIRS})

# Lier OpenCV si disponible
if(OpenCV_FOUND)
    target_link_libraries(vision-service ${OpenCV_LIBS})
    # Ajouter curl pour résoudre les dépendances GDAL/HDF5
    find_package(CURL REQUIRED)
    target_link_libraries(vision-service CURL::libcurl)
endif()

# Installation
install(TARGETS vision-service DESTINATION bin)

# Tests (optionnel)
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        enable_testing()
        
        # Sources des tests
        set(TEST_SOURCES
            tests/test_vision_service.cpp
            src/vision_service.cpp
            src/service_metrics.cpp
            src/frame_processor.cpp
            src/camera_manager.cpp
            ${PROTO_SRCS}
            ${GRPC_SRCS}
        )
        
        # Add OpenCV-specific sources to tests if available
        if(OpenCV_FOUND)
            list(APPEND TEST_SOURCES
                src/opencv_capture_manager.cpp
                src/opencv_motion_detector.cpp
                src/performance_manager.cpp
            )
        endif()
        
        # Exécutable de tests
        add_executable(vision-service-tests ${TEST_SOURCES})
        
        # Liens pour les tests - ADD grpc++_reflection and re2
        target_link_libraries(vision-service-tests
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
            ${RE2_LIBRARIES}
            GTest::gtest
            GTest::gtest_main
            pthread
        )
        
        # Include directories for RE2
        target_include_directories(vision-service-tests PRIVATE ${RE2_INCLUDE_DIRS})
        
        # Lier OpenCV si disponible
        if(OpenCV_FOUND)
            target_link_libraries(vision-service-tests ${OpenCV_LIBS})
            # Ajouter curl pour résoudre les dépendances GDAL/HDF5
            target_link_libraries(vision-service-tests CURL::libcurl)
        endif()
        
        # Registrer les tests
        add_test(NAME VisionServiceTests COMMAND vision-service-tests)
        
        message(STATUS "Tests enabled - run with 'make test'")
    else()
        message(STATUS "GTest not found - tests disabled")
    endif()
endif()

# Affichage de la configuration
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Protobuf version: ${Protobuf_VERSION}")
message(STATUS "  gRPC found: ${gRPC_FOUND}")
message(STATUS "  OpenCV found: ${OpenCV_FOUND}")
if(GTest_FOUND AND BUILD_TESTS)
    message(STATUS "  Tests: Enabled")  
else()
    message(STATUS "  Tests: Disabled")
endif()
message(STATUS "")
# OpenCV Configuration for Phase 2.3
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
    add_definitions(-DHAVE_OPENCV)
    include_directories(${OpenCV_INCLUDE_DIRS})
    
    # Ajouter les composants OpenCV au target principal
    target_link_libraries(vision-service ${OpenCV_LIBS})
    
    # Ajouter aux tests aussi
    if(BUILD_TESTS AND GTest_FOUND)
        target_link_libraries(vision-service-tests ${OpenCV_LIBS})
    endif()
    
    # Afficher les composants OpenCV
    message(STATUS "OpenCV components: ${OpenCV_LIBS}")
else()
    message(FATAL_ERROR "OpenCV not found - required for Phase 2.3")
endif()

# Optimisations spécifiques OpenCV
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(vision-service PRIVATE CV_ENABLE_INTRINSICS)
endif()

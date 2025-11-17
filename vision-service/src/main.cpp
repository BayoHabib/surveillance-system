// src/main.cpp
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "vision_service.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Variable globale pour l'arrêt gracieux
volatile bool running = true;

// Handler pour les signaux (SIGINT, SIGTERM)
void signalHandler(int signal) {
    std::cout << "\n🛑 Signal reçu (" << signal << "), arrêt en cours..." << std::endl;
    running = false;
}

int main(int argc, char** argv) {
    // Configuration par défaut
    std::string server_address = "0.0.0.0:50051";
    
    // Parse arguments simples
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Vision Service C++ - Surveillance System\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>    Port d'écoute (défaut: 50051)\n";
            std::cout << "  --host <host>    Adresse d'écoute (défaut: 0.0.0.0)\n";
            std::cout << "  --help, -h       Afficher cette aide\n";
            std::cout << "  --version, -v    Afficher la version\n\n";
            std::cout << "Exemples:\n";
            std::cout << "  " << argv[0] << "                    # Écoute sur 0.0.0.0:50051\n";
            std::cout << "  " << argv[0] << " --port 8080       # Écoute sur 0.0.0.0:8080\n";
            std::cout << "  " << argv[0] << " --host localhost  # Écoute sur localhost:50051\n";
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "Vision Service v1.0.0\n";
            std::cout << "gRPC Vision Processing Service for Surveillance System\n";
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            server_address = "0.0.0.0:" + std::string(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            std::string host = argv[++i];
            // Extraire le port s'il existe dans server_address
            size_t colon_pos = server_address.find(':');
            std::string port = (colon_pos != std::string::npos) ? 
                               server_address.substr(colon_pos) : ":50051";
            server_address = host + port;
        }
    }
    
    // Installation des handlers de signaux
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "🎥 Vision Service - Démarrage...\n" << std::endl;
    
    // Créer le service
    VisionServiceImpl service;
    
    // Activer la réflexion gRPC (pour le debugging)
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    // Configuration du serveur gRPC
    ServerBuilder builder;
    
    // Écouter sur l'adresse spécifiée
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Enregistrer le service
    builder.RegisterService(&service);
    
    // Activer le health check service
    grpc::EnableDefaultHealthCheckService(true);
    
    // Construire et démarrer le serveur
    std::unique_ptr<Server> server(builder.BuildAndStart());
    
    if (!server) {
        std::cerr << "❌ Erreur: Impossible de démarrer le serveur sur " << server_address << std::endl;
        return 1;
    }
    
    std::cout << "✅ Vision Service démarré avec succès" << std::endl;
    std::cout << "🌐 Écoute sur: " << server_address << std::endl;
    std::cout << "📡 Service gRPC: surveillance.vision.VisionService" << std::endl;
    std::cout << "🔧 Health Check: activé" << std::endl;
    std::cout << "🔍 Réflexion gRPC: activée" << std::endl;
    std::cout << "\n💡 Utilisez Ctrl+C pour arrêter le service\n" << std::endl;
    
    // Afficher les endpoints disponibles
    std::cout << "📋 Endpoints disponibles:" << std::endl;
    std::cout << "  - StartStream: Démarrer un stream de caméra" << std::endl;
    std::cout << "  - StopStream: Arrêter un stream de caméra" << std::endl;
    std::cout << "  - GetStreamStatus: Statut d'un stream" << std::endl;
    std::cout << "  - GetHealth: Health check du service" << std::endl;
    std::cout << "  - ProcessFrames: Traitement de frames (streaming)" << std::endl;
    std::cout << std::endl;
    
    // Boucle principale avec monitoring
    auto start_time = std::chrono::steady_clock::now();
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Afficher des stats périodiquement (toutes les 30 secondes)
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        
        if (uptime.count() % 30 == 0 && uptime.count() > 0) {
            std::cout << "📊 Uptime: " << uptime.count() << "s, "
                      << "Streams actifs: " << service.GetActiveStreamsCount() 
                      << std::endl;
        }
    }
    
    std::cout << "\n🔄 Arrêt du serveur..." << std::endl;
    
    // Arrêt gracieux
    server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(5));
    
    std::cout << "✅ Vision Service arrêté proprement" << std::endl;
    
    return 0;
}
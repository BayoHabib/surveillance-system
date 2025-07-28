# 🎥 Surveillance System - AI-Powered Security Platform

> **Phase 1 MVP** - Système de surveillance intelligent avec IA pour centres de détention et sécurité

[![Go Version](https://img.shields.io/badge/Go-1.21+-blue.svg)](https://golang.org)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Docker](https://img.shields.io/badge/Docker-Ready-blue.svg)](Dockerfile)

## 🚀 Vue d'Ensemble

Ce projet implémente un système de surveillance avancé utilisant l'IA pour la détection automatique d'événements de sécurité. Conçu initialement pour les centres de détention, il offre une architecture modulaire extensible pour divers environnements sécurisés.

### ✨ Fonctionnalités Actuelles (Phase 1)

- 🎯 **Dashboard Temps Réel** - Interface web responsive avec contrôles multi-caméras
- 🔌 **WebSocket** - Communication bidirectionnelle temps réel
- 🤖 **Détection IA** - Système modulaire de détecteurs (mouvement, intrusion, reconnaissance)
- 🚨 **Alertes Intelligentes** - Système d'alertes avec niveaux de priorité
- 📊 **Monitoring** - Métriques de performance et logs temps réel
- 🐳 **Containerisé** - Déploiement Docker/Docker Compose
- 🏗️ **Architecture Modulaire** - Prêt pour l'évolution (C++, WASM, IA avancée)

## 🏛️ Architecture

```
┌─────────────────────────────────────────────────┐
│ React Dashboard (WebGL + WebSocket)             │
└─────────────────┬───────────────────────────────┘
                  │ WebSocket + REST API
┌─────────────────▼───────────────────────────────┐
│ Go Orchestrateur                                │
│ ├─ APIs REST + WebSocket Hub                    │
│ ├─ Event Processor                              │
│ └─ Alert Manager                                │
└─────────────────┬───────────────────────────────┘
                  │ gRPC (prévu)
┌─────────────────▼───────────────────────────────┐
│ Vision Service (Mock → C++ prévu)               │
│ └─ Détection + Traitement vidéo                 │
└─────────────────┬───────────────────────────────┘
                  │ Binary + Metadata
┌─────────────────▼───────────────────────────────┐
│ IA Modules (WASM + ONNX prévus)                 │
│ └─ Reconnaissance faciale + Analyse comportement│
└─────────────────────────────────────────────────┘
```

## 🛠️ Stack Technique

- **Backend**: Go 1.21+ (Gin, WebSocket, gRPC-ready)
- **Frontend**: Vanilla JS + WebGL (React prévu Phase 2)
- **Vision**: OpenCV + C++ (Phase 2)
- **IA**: WASM + ONNX Runtime (Phase 3)
- **Database**: In-memory → PostgreSQL (Phase 2)
- **Déploiement**: Docker + Docker Compose

## 🚀 Installation Rapide

### Prérequis
- Go 1.21+
- Docker (optionnel)

### Démarrage Local

```bash
# Cloner le projet
git clone https://github.com/[username]/surveillance-system
cd surveillance-system

# Installer les dépendances
go mod tidy

# Lancer l'application
go run cmd/server/main.go

# Ou avec hot-reload (dev)
make dev
```

**Dashboard:** http://localhost:8080

### Démarrage Docker

```bash
# Build et run
docker-compose up -d

# Voir les logs
docker-compose logs -f
```

## 📊 API Endpoints

### Caméras
- `GET /api/v1/cameras` - Liste des caméras
- `POST /api/v1/cameras` - Créer une caméra
- `PUT /api/v1/cameras/:id/start` - Démarrer le stream
- `PUT /api/v1/cameras/:id/stop` - Arrêter le stream

### Alertes
- `GET /api/v1/alerts` - Liste des alertes
- `GET /api/v1/alerts?camera_id=xxx` - Alertes par caméra

### Système
- `GET /api/v1/health` - État système + métriques
- `WS /ws` - WebSocket temps réel

## 🧪 Tests et Simulation

Le système inclut un **mock intelligent** simulant :
- 📹 Streams vidéo multi-caméras (15fps)
- 🔍 Détections aléatoires réalistes
- 🚨 Génération d'alertes automatique
- ⚡ Performance temps réel

```bash
# Tests API
curl http://localhost:8080/api/v1/health
curl -X PUT http://localhost:8080/api/v1/cameras/cam_001/start
```

## 🗺️ Roadmap

### Phase 2 (En Cours) - Vision Avancée
- [ ] Service C++ avec OpenCV
- [ ] Traitement vidéo temps réel
- [ ] Détection de mouvement optimisée
- [ ] Base de données PostgreSQL

### Phase 3 - IA & Reconnaissance
- [ ] Modules WASM + ONNX
- [ ] Reconnaissance faciale
- [ ] Analyse comportementale
- [ ] Détection d'objets avancée

### Phase 4 - Production
- [ ] Authentification & autorisation
- [ ] Monitoring Prometheus/Grafana
- [ ] Tests e2e complets
- [ ] Documentation API complète

## 🏗️ Structure du Projet

```
surveillance-system/
├── cmd/server/          # Point d'entrée
├── internal/
│   ├── api/            # Handlers REST
│   ├── core/           # Logique métier
│   ├── vision/         # Client vision (mock)
│   └── websocket/      # Hub temps réel
├── web/                # Frontend
├── docker-compose.yml  # Orchestration
├── Makefile           # Commandes utiles
└── README.md          # Cette doc
```

## 🤝 Contribution

1. Fork le projet
2. Créer une branche feature (`git checkout -b feature/amazing-feature`)
3. Commit (`git commit -m 'Add amazing feature'`)
4. Push (`git push origin feature/amazing-feature`)
5. Ouvrir une Pull Request

## 📄 Licence

Ce projet est sous licence MIT. Voir [LICENSE](LICENSE) pour plus de détails.

## 🎯 Cas d'Usage

- **Centres de Détention** - Surveillance sécurisée avec détection d'incidents
- **Entreprises** - Contrôle d'accès et sécurité périmètrique  
- **Espaces Publics** - Monitoring intelligent et alertes automatiques
- **Résidentiel** - Surveillance domestique avancée

---

## 📞 Support

- 📧 Issues: [GitHub Issues](../../issues)
- 📖 Documentation: [Wiki](../../wiki)
- 🚀 Démo Live: À venir

**🎉 Projet en développement actif - Contributions bienvenues !**

# 🔌 Problématique RTSP avec WSL - Analyse & Solutions

## 📋 Contexte

Le projet tourne actuellement sous **WSL 2 (Windows Subsystem for Linux)** sur Windows. Le service C++ `vision-service` doit se connecter à des caméras RTSP sur le réseau local, mais **WSL 2 a des limitations réseau critiques** qui rendent les connexions RTSP instables ou impossibles.

## 🚨 Problème Identifié

### Symptômes Observés
- ❌ Connexions RTSP qui timeout (même avec backoff exponentiel implémenté - Bug #8)
- ❌ `opencv_capture_->open("rtsp://...")` échoue systématiquement
- ❌ Perte de paquets sur flux UDP/TCP
- ❌ Multicast non supporté
- ⚠️ Les streams fonctionnent sur Windows natif mais pas dans WSL

### Causes Racines

#### 1. **Architecture Réseau WSL 2** 🌐
WSL 2 utilise une **VM Hyper-V légère** avec son propre réseau virtualisé :

```
┌─────────────────────────────────────────┐
│ Windows Host (192.168.1.100)            │
│                                         │
│  ┌───────────────────────────────────┐  │
│  │ WSL 2 VM (172.x.x.x NAT privé)   │  │
│  │                                   │  │
│  │  ┌─────────────────────────────┐ │  │
│  │  │ vision-service (C++)        │ │  │
│  │  │ opencv_capture_->open()     │ │  │
│  │  └─────────────────────────────┘ │  │
│  │                                   │  │
│  │  NAT + vSwitch Hyper-V           │  │
│  └───────────────────────────────────┘  │
│                ↓ (NAT traversal)        │
└─────────────────────────────────────────┘
                 ↓
         ┌───────────────┐
         │ Caméra RTSP   │
         │ 192.168.1.50  │
         │ Port 554      │
         └───────────────┘
```

**Problèmes** :
- NAT ajoute latence (50-200ms)
- Perte paquets UDP (RTSP utilise RTP/UDP)
- Firewall Windows peut bloquer
- Pas de multicast routing

#### 2. **Protocole RTSP/RTP Complexe** 📡

RTSP n'est pas un simple protocole TCP :

```
RTSP Session:
1. TCP 554 (Control) ✅ Passe le NAT WSL
2. UDP 5004-5005 (RTP/RTCP Data) ❌ Bloqué par NAT
3. Multicast (optionnel) ❌ Non routé dans WSL
```

**Exemple de flux** :
```
Client → RTSP Server (TCP 554):
  OPTIONS, DESCRIBE, SETUP, PLAY

Server → Client (UDP 5004+):
  H.264/H.265 frames (RTP)
  ❌ Ces paquets UDP n'arrivent pas dans WSL
```

#### 3. **Limitations OpenCV avec WSL** 🎥

OpenCV utilise **FFmpeg** ou **GStreamer** pour RTSP :

```cpp
// opencv_capture_->open("rtsp://192.168.1.50:554/stream")
// Appelle en interne:
// ffmpeg -rtsp_transport tcp -i rtsp://... (si TCP forcé)
// ffmpeg -i rtsp://... (UDP par défaut ❌)
```

**Problème** : Par défaut, FFmpeg préfère **UDP** qui ne fonctionne pas bien dans WSL 2.

## 💡 Solutions Possibles

### Solution 1 : **Forcer RTSP sur TCP** (Quick Fix) ⚡

**Principe** : Utiliser RTSP sur TCP uniquement, éviter UDP.

**Implémentation** :

```cpp
// Dans opencv_capture_manager.cpp - SetupRtspCapture()

bool OpenCVCaptureManager::SetupRtspCapture() {
    LOG_INFO("[OpenCVCaptureManager] Setting up RTSP capture with TCP transport");
    
    // Option 1: Environment variable pour FFmpeg
    #ifdef _WIN32
        _putenv("OPENCV_FFMPEG_CAPTURE_OPTIONS=rtsp_transport;tcp");
    #else
        setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp", 1);
    #endif
    
    // Option 2: Modifier l'URL RTSP
    std::string tcp_url = camera_url_;
    if (camera_url_.find("?") == std::string::npos) {
        tcp_url += "?tcp";  // Ajoute option TCP
    }
    
    // Option 3: Utiliser cv::CAP_FFMPEG avec paramètres
    opencv_capture_ = std::make_unique<VideoCapture>();
    
    // Forcer backend FFmpeg
    if (!opencv_capture_->open(camera_url_, cv::CAP_FFMPEG)) {
        LOG_ERROR("[OpenCVCaptureManager] Failed to open RTSP with FFmpeg backend");
        return false;
    }
    
    // Configurer options de capture
    opencv_capture_->set(cv::CAP_PROP_BUFFERSIZE, 1);  // Réduire latence
    opencv_capture_->set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000);
    opencv_capture_->set(cv::CAP_PROP_READ_TIMEOUT_MSEC, 5000);
    
    LOG_INFO("[OpenCVCaptureManager] RTSP capture configured with TCP transport");
    return true;
}
```

**Avantages** :
- ✅ Quick fix (10-15 minutes)
- ✅ TCP traverse mieux le NAT WSL
- ✅ Pas de paquets perdus
- ✅ Compatible avec toutes les caméras

**Inconvénients** :
- ⚠️ Latence plus élevée (TCP overhead)
- ⚠️ Buffering possible
- ⚠️ Moins de throughput qu'UDP

### Solution 2 : **Utiliser GStreamer au lieu d'OpenCV** 🎬

**Principe** : GStreamer a un meilleur support RTSP et plus d'options de configuration.

**Implémentation** :

```cpp
// Dans CMakeLists.txt, ajouter:
find_package(GStreamer REQUIRED)

// Nouveau fichier: gstreamer_capture_manager.h
class GStreamerCaptureManager : public CameraManager {
public:
    bool Initialize() override {
        std::string pipeline = 
            "rtspsrc location=" + camera_url_ + 
            " protocols=tcp latency=0 ! "
            "rtph264depay ! h264parse ! avdec_h264 ! "
            "videoconvert ! video/x-raw,format=BGR ! "
            "appsink name=sink";
        
        cap_ = cv::VideoCapture(pipeline, cv::CAP_GSTREAMER);
        return cap_.isOpened();
    }
    
private:
    cv::VideoCapture cap_;
};
```

**Avantages** :
- ✅ Contrôle fin du pipeline RTSP
- ✅ Meilleure gestion des erreurs
- ✅ Supporte RTSP/RTP/RTCP advanced
- ✅ Options de buffering configurables

**Inconvénients** :
- ⚠️ Dépendance supplémentaire (GStreamer)
- ⚠️ Complexité accrue
- ⚠️ 1-2 heures d'implémentation

### Solution 3 : **Port Forwarding WSL → Windows** 🔀

**Principe** : Créer un proxy RTSP sur Windows qui forward vers WSL.

**Implémentation** :

```powershell
# Sur Windows (PowerShell Admin)
# Forward port 554 (RTSP) vers WSL
$wslIP = (wsl hostname -I).Trim()
netsh interface portproxy add v4tov4 `
    listenaddress=0.0.0.0 `
    listenport=8554 `
    connectaddress=$wslIP `
    connectport=8554

# Dans WSL, lancer un proxy RTSP simple
# Option: utiliser rtsp-simple-server
docker run --rm -it -p 8554:8554 aler9/rtsp-simple-server

# Configurer caméra pour streamer vers Windows:8554
# vision-service se connecte à localhost:8554
```

**Avantages** :
- ✅ Contourne NAT WSL
- ✅ Peut utiliser UDP côté Windows
- ✅ Fonctionne pour plusieurs caméras

**Inconvénients** :
- ⚠️ Infrastructure complexe
- ⚠️ Point de défaillance supplémentaire
- ⚠️ Latence ajoutée (double hop)

### Solution 4 : **Migrer vers WSL 1 (Mirrored Mode)** 🪞

**Principe** : WSL 1 utilise le réseau Windows directement (pas de NAT).

**Implémentation** :

```bash
# Convertir distro en WSL 1
wsl --set-version Ubuntu 1

# Ou utiliser WSL 2 Mirrored Mode (Windows 11 22H2+)
# Dans %UserProfile%\.wslconfig:
[wsl2]
networkingMode=mirrored
dnsTunneling=true
firewall=true
autoProxy=true
```

**Avantages** :
- ✅ Accès direct au réseau Windows
- ✅ Pas de NAT traversal
- ✅ Multicast supporté
- ✅ UDP fonctionne parfaitement

**Inconvénients** :
- ⚠️ WSL 1 : performances I/O dégradées
- ⚠️ Mirrored Mode : Windows 11 uniquement
- ⚠️ Certaines fonctionnalités Linux limitées en WSL 1

### Solution 5 : **Docker Desktop avec Host Network** 🐳

**Principe** : Utiliser le mode réseau `host` de Docker qui contourne le NAT.

**Implémentation** :

```yaml
# docker-compose.yml
version: '3.8'
services:
  vision-service:
    build: ./vision-service
    network_mode: "host"  # ✅ Accès direct réseau
    environment:
      - RTSP_URL=rtsp://192.168.1.50:554/stream
    volumes:
      - ./videos:/app/videos
```

**Avantages** :
- ✅ Accès réseau natif
- ✅ Pas de port mapping
- ✅ Multicast supporté
- ✅ Performances maximales

**Inconvénients** :
- ⚠️ `network_mode: host` ne marche pas bien sur Windows/Mac Docker Desktop
- ⚠️ Perte d'isolation réseau
- ⚠️ Linux uniquement (vraiment efficace)

### Solution 6 : **Compilation Native Windows** 🪟

**Principe** : Compiler `vision-service` nativement pour Windows (pas WSL).

**Implémentation** :

```bash
# Installer MSYS2 ou Visual Studio
# Installer OpenCV pour Windows
vcpkg install opencv4:x64-windows

# Compiler
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# Lancer directement sur Windows
./Release/vision-service.exe
```

**Avantages** :
- ✅ Performances maximales
- ✅ Accès réseau natif Windows
- ✅ Pas de problème RTSP/UDP
- ✅ Production-ready

**Inconvénients** :
- ⚠️ Environnement de build différent
- ⚠️ Perd l'uniformité Linux
- ⚠️ 2-3 heures setup initial

## 🎯 Recommandation Finale

### Pour Développement Rapide (Maintenant)

**Solution 1 : Forcer RTSP sur TCP** ⭐⭐⭐⭐⭐

**Pourquoi** :
- Quickest fix (15 minutes)
- Résout 90% des problèmes WSL/RTSP
- Aucune infrastructure additionnelle
- Fonctionne avec OpenCV existant

**Comment** :
```cpp
// 1. Ajouter dans opencv_capture_manager.cpp
bool OpenCVCaptureManager::SetupRtspCapture() {
    // Force TCP transport
    setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp", 1);
    
    opencv_capture_ = std::make_unique<VideoCapture>();
    opencv_capture_->set(cv::CAP_PROP_BUFFERSIZE, 1);
    opencv_capture_->set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 10000);
    
    if (!opencv_capture_->open(camera_url_, cv::CAP_FFMPEG)) {
        LOG_ERROR("RTSP TCP open failed");
        return false;
    }
    
    return opencv_capture_->isOpened();
}

// 2. Appeler dans Initialize()
if (source_type_ == SourceType::RTSP) {
    return SetupRtspCapture();
}
```

**Test** :
```bash
# Avec caméra RTSP réelle
./vision-service --camera=rtsp://admin:pass@192.168.1.50:554/stream1

# Avec simulateur RTSP
docker run --rm -p 8554:8554 aler9/rtsp-simple-server
./vision-service --camera=rtsp://localhost:8554/test
```

### Pour Production (Long Terme)

**Solution 6 : Build Windows Natif + Solution 2 : GStreamer** ⭐⭐⭐⭐⭐

**Pourquoi** :
- Performances optimales
- Stabilité maximale
- Contrôle fin RTSP
- Pas de limitations WSL

**Plan de Migration** :
1. **Phase 1** (1 semaine) : Setup build Windows natif
2. **Phase 2** (3-5 jours) : Intégrer GStreamer pipeline
3. **Phase 3** (2 jours) : Tests avec caméras réelles
4. **Phase 4** (1 jour) : Documentation + CI/CD

## 🧪 Tests & Validation

### Test avec Simulateur RTSP

```bash
# 1. Lancer serveur RTSP test
docker run --rm \
  -p 8554:8554 \
  -v $(pwd)/test-video.mp4:/video.mp4 \
  aler9/rtsp-simple-server

# 2. Publier vidéo test
ffmpeg -re -stream_loop -1 \
  -i test-video.mp4 \
  -c copy -f rtsp rtsp://localhost:8554/test

# 3. Tester vision-service
./vision-service --camera=rtsp://localhost:8554/test --log-level=DEBUG

# 4. Vérifier logs
grep "RTSP" logs/vision-service.log
grep "Frame captured" logs/vision-service.log
```

### Test avec Caméra Réelle

```bash
# 1. Scanner réseau pour caméras
nmap -p 554 192.168.1.0/24

# 2. Tester connexion avec VLC
vlc rtsp://192.168.1.50:554/stream1

# 3. Tester avec FFmpeg
ffmpeg -rtsp_transport tcp -i rtsp://192.168.1.50:554/stream1 \
  -frames:v 10 output%03d.jpg

# 4. Tester avec vision-service
./vision-service --camera=rtsp://192.168.1.50:554/stream1
```

### Diagnostic des Problèmes

```bash
# Vérifier connectivité réseau WSL → Windows
ping 192.168.1.50

# Vérifier port RTSP ouvert
nc -zv 192.168.1.50 554
telnet 192.168.1.50 554

# Capturer paquets réseau (tcpdump)
sudo tcpdump -i eth0 -w rtsp.pcap port 554
# Analyser avec Wireshark

# Vérifier FFmpeg support
ffmpeg -protocols | grep rtsp
ffmpeg -formats | grep rtp

# Test OpenCV direct
python3 -c "
import cv2
cap = cv2.VideoCapture('rtsp://localhost:8554/test')
print('Opened:', cap.isOpened())
ret, frame = cap.read()
print('Read:', ret, frame.shape if ret else None)
"
```

## 📊 Tableau Comparatif des Solutions

| Solution | Complexité | Temps Setup | Fiabilité | Performance | Production Ready |
|----------|------------|-------------|-----------|-------------|------------------|
| **1. Force TCP** | ⭐ | 15 min | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **2. GStreamer** | ⭐⭐⭐ | 2h | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| **3. Port Forward** | ⭐⭐⭐⭐ | 1h | ⭐⭐ | ⭐⭐ | ⭐⭐ |
| **4. WSL Mirrored** | ⭐⭐ | 30 min | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| **5. Docker Host** | ⭐⭐ | 30 min | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| **6. Native Windows** | ⭐⭐⭐⭐ | 3h | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

## 🚀 Plan d'Action Immédiat

### Étape 1 : Quick Fix (15 minutes)
```cpp
// Implémenter Force TCP dans opencv_capture_manager.cpp
```

### Étape 2 : Tester (30 minutes)
```bash
# Avec simulateur + caméra réelle
```

### Étape 3 : Documenter (15 minutes)
```markdown
# Ajouter section RTSP dans README
```

### Étape 4 : Décider Long Terme (1 semaine)
- Évaluer build Windows natif
- Tester GStreamer
- Choisir architecture production

---

**Dernière mise à jour** : Novembre 2025  
**Priorité** : P1 (Critique pour fonctionnalité RTSP)  
**Status** : Solution Quick Fix disponible, plan long terme défini

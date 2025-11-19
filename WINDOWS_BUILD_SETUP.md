# 🪟 Guide d'installation - Build Windows Natif

Ce guide permet de compiler **vision-service** directement sur Windows (sans WSL).

## 📋 Prérequis

### Étape 1 : Visual Studio Build Tools

**Option A - Visual Studio Community (Recommandé)** :
1. Télécharger : https://visualstudio.microsoft.com/fr/downloads/
2. Installer **Visual Studio 2022 Community**
3. Dans l'installeur, sélectionner :
   - ✅ **Développement Desktop en C++**
   - ✅ **SDK Windows 10/11**
   - ✅ **CMake Tools**
   - ✅ **Outils de build MSVC v143**

**Option B - Build Tools standalone** :
1. Télécharger : https://visualstudio.microsoft.com/fr/downloads/#build-tools-for-visual-studio-2022
2. Installer avec les mêmes composants

**Taille** : ~7 GB  
**Temps** : 15-30 minutes

---

### Étape 2 : CMake

**Option A - Avec Visual Studio** : Déjà inclus ✅

**Option B - Standalone** :
```powershell
# Via Chocolatey
choco install cmake -y

# OU télécharger depuis
# https://cmake.org/download/
# Installer en cochant "Add CMake to PATH"
```

Vérifier :
```powershell
cmake --version
# Devrait afficher cmake version 3.x.x
```

---

### Étape 3 : vcpkg (Package Manager C++)

```powershell
# 1. Cloner vcpkg
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# 2. Bootstrap
.\bootstrap-vcpkg.bat

# 3. Intégrer avec Visual Studio
.\vcpkg integrate install

# 4. Ajouter au PATH (optionnel)
$env:PATH += ";C:\vcpkg"
[Environment]::SetEnvironmentVariable("PATH", $env:PATH, "User")

# 5. Définir VCPKG_ROOT
$env:VCPKG_ROOT = "C:\vcpkg"
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
```

**Taille** : ~500 MB  
**Temps** : 5-10 minutes

---

### Étape 4 : Installer les dépendances C++

```powershell
cd C:\vcpkg

# OpenCV avec FFmpeg (support RTSP)
.\vcpkg install opencv4[contrib,ffmpeg]:x64-windows

# gRPC + Protobuf
.\vcpkg install grpc:x64-windows
.\vcpkg install protobuf:x64-windows

# Utilitaires
.\vcpkg install curl:x64-windows
```

**Taille totale** : ~5 GB  
**Temps** : 1-2 heures (compilation depuis sources)

⏳ **C'est long** - vcpkg compile tout depuis les sources. Lancez et allez prendre un café !

---

### Étape 5 : Git (si pas déjà installé)

```powershell
choco install git -y
# OU télécharger depuis https://git-scm.com/
```

---

## 🔨 Compilation de vision-service

### 1. Configurer CMake avec vcpkg

```powershell
cd C:\Users\Administrator\git_clone\surveillance-system\vision-service

# Créer dossier build
mkdir build-windows
cd build-windows

# Configurer avec vcpkg toolchain
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release
```

### 2. Compiler

```powershell
# Option A : Avec CMake
cmake --build . --config Release -j 8

# Option B : Avec MSBuild
msbuild vision-service.sln /p:Configuration=Release /m
```

### 3. Exécuter

```powershell
.\Release\vision-service.exe --port=50051 --log-level=DEBUG
```

---

## 🎥 Test RTSP avec caméra réelle

```powershell
# Démarrer vision-service
.\Release\vision-service.exe --port=50051

# Dans une autre console PowerShell
cd C:\Users\Administrator\git_clone\surveillance-system

# Démarrer surveillance-core (Go)
go run cmd/server/main.go

# Ouvrir navigateur
start http://localhost:8080
```

Dans l'interface web, ajouter une caméra RTSP avec une vraie URL :
```
rtsp://admin:password@192.168.1.100:554/stream1
```

---

## 📊 Résumé des installations

| Composant | Taille | Temps | Priorité |
|-----------|--------|-------|----------|
| Visual Studio Build Tools | ~7 GB | 20 min | ⭐⭐⭐ |
| vcpkg | ~500 MB | 5 min | ⭐⭐⭐ |
| OpenCV (vcpkg) | ~3 GB | 45 min | ⭐⭐⭐ |
| gRPC (vcpkg) | ~2 GB | 30 min | ⭐⭐⭐ |
| **TOTAL** | **~12 GB** | **1h30-2h** | |

---

## ⚡ Installation rapide (tout-en-un)

```powershell
# Script PowerShell automatique
# Sauvegarder sous install-vision-deps.ps1

# Visual Studio Build Tools (manuel)
Write-Host "1. Installer Visual Studio depuis le navigateur..." -ForegroundColor Yellow
Start-Process "https://visualstudio.microsoft.com/downloads/"
Read-Host "Appuyez sur Entrée après installation de Visual Studio"

# vcpkg
Write-Host "`n2. Installation vcpkg..." -ForegroundColor Green
cd C:\
git clone https://github.com/microsoft/vcpkg.git 2>$null
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Variables d'environnement
$env:VCPKG_ROOT = "C:\vcpkg"
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")

# Dépendances C++
Write-Host "`n3. Installation dépendances (1-2h)..." -ForegroundColor Yellow
.\vcpkg install opencv4[contrib,ffmpeg]:x64-windows --recurse
.\vcpkg install grpc:x64-windows --recurse
.\vcpkg install protobuf:x64-windows --recurse

Write-Host "`n✅ Installation terminée!" -ForegroundColor Green
Write-Host "Redémarrer le terminal puis compiler vision-service" -ForegroundColor Cyan
```

---

## 🐛 Dépannage

### Erreur : "LINK : fatal error LNK1104: cannot open file"
**Solution** : Exécuter depuis "Developer Command Prompt for VS 2022"

### Erreur : "CMake could not find vcpkg"
**Solution** : 
```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

### Erreur : "OpenCV not found"
**Solution** :
```powershell
# Vérifier installation
C:\vcpkg\vcpkg list | Select-String opencv

# Réinstaller si nécessaire
C:\vcpkg\vcpkg install opencv4[contrib,ffmpeg]:x64-windows --recurse
```

### Build très lent
**Solution** : Utiliser compilation parallèle
```powershell
cmake --build . --config Release -j 12  # 12 threads
```

---

## 🎯 Avantages du build Windows natif

✅ **Pas de limitations WSL** - Accès direct réseau, pas de NAT  
✅ **Performances natives** - Pas d'overhead VM  
✅ **Support RTSP complet** - UDP + TCP sans contraintes  
✅ **Hardware acceleration** - CUDA, OpenCL possible  
✅ **Debugging Visual Studio** - Outils professionnels  

---

## 🔗 Liens utiles

- Visual Studio : https://visualstudio.microsoft.com/
- vcpkg : https://github.com/microsoft/vcpkg
- OpenCV : https://opencv.org/
- gRPC : https://grpc.io/
- CMake : https://cmake.org/

---

## 📝 Notes

- Une fois compilé, `vision-service.exe` est portable (avec les DLLs)
- Les DLLs sont dans `build-windows\Release\` et `C:\vcpkg\installed\x64-windows\bin\`
- Pour distribution : copier toutes les DLLs avec l'exe

**Prochaine étape** : Après compilation, tester avec vraie caméra RTSP pour valider le fix TCP !

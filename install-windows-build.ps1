# ========================================
# Script d'installation automatique
# Build Windows Natif - Vision Service
# ========================================

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Vision Service - Windows Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ========================================
# 1. Vérifier Visual Studio
# ========================================
Write-Host "[1/5] Vérification Visual Studio..." -ForegroundColor Yellow

$vsPath = "C:\Program Files\Microsoft Visual Studio\2022"
$vsBuildTools = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"

if (Test-Path $vsPath) {
    Write-Host "  [OK] Visual Studio 2022 trouve" -ForegroundColor Green
} elseif (Test-Path $vsBuildTools) {
    Write-Host "  [OK] VS Build Tools 2022 trouve" -ForegroundColor Green
} else {
    Write-Host "  [X] Visual Studio non installe" -ForegroundColor Red
    Write-Host ""
    Write-Host "Installation requise:" -ForegroundColor Yellow
    Write-Host "  1. Télécharger: https://visualstudio.microsoft.com/downloads/" -ForegroundColor White
    Write-Host "  2. Installer 'Développement Desktop en C++'" -ForegroundColor White
    Write-Host "  3. Relancer ce script" -ForegroundColor White
    Write-Host ""
    
    $response = Read-Host "Ouvrir le lien de téléchargement? (O/N)"
    if ($response -eq "O") {
        Start-Process "https://visualstudio.microsoft.com/downloads/"
    }
    exit 1
}

# ========================================
# 2. Installer/Vérifier vcpkg
# ========================================
Write-Host ""
Write-Host "[2/5] Installation vcpkg..." -ForegroundColor Yellow

$vcpkgRoot = "C:\vcpkg"

if (Test-Path $vcpkgRoot) {
    Write-Host "  [OK] vcpkg deja installe" -ForegroundColor Green
} else {
    Write-Host "  -> Clonage vcpkg..." -ForegroundColor White
    
    Push-Location C:\
    git clone https://github.com/microsoft/vcpkg.git
    Pop-Location
    
    Write-Host "  -> Bootstrap vcpkg..." -ForegroundColor White
    Push-Location $vcpkgRoot
    .\bootstrap-vcpkg.bat
    .\vcpkg integrate install
    Pop-Location
    
    Write-Host "  [OK] vcpkg installe" -ForegroundColor Green
}

# Variables d'environnement
$env:VCPKG_ROOT = $vcpkgRoot
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "User")

# ========================================
# 3. Vérifier dépendances installées
# ========================================
Write-Host ""
Write-Host "[3/5] Vérification dépendances..." -ForegroundColor Yellow

Push-Location $vcpkgRoot

$packages = @{
    "opencv4" = "opencv4[contrib,ffmpeg]:x64-windows"
    "grpc" = "grpc:x64-windows"
    "protobuf" = "protobuf:x64-windows"
}

$toInstall = @()

foreach ($pkg in $packages.Keys) {
    $installed = .\vcpkg list | Select-String $pkg
    if ($installed) {
        Write-Host "  [OK] $pkg installe" -ForegroundColor Green
    } else {
        Write-Host "  [X] $pkg manquant" -ForegroundColor Yellow
        $toInstall += $packages[$pkg]
    }
}

# ========================================
# 4. Installer dépendances manquantes
# ========================================
if ($toInstall.Count -gt 0) {
    Write-Host ""
    Write-Host "[4/5] Installation dépendances..." -ForegroundColor Yellow
    Write-Host "  ⏳ Cela peut prendre 1-2 heures..." -ForegroundColor Cyan
    Write-Host ""
    
    foreach ($pkg in $toInstall) {
        Write-Host "  -> Installation: $pkg" -ForegroundColor White
        .\vcpkg install $pkg --recurse
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  [OK] $pkg installe avec succes" -ForegroundColor Green
        } else {
            Write-Host "  [X] Erreur installation $pkg" -ForegroundColor Red
            exit 1
        }
    }
} else {
    Write-Host ""
    Write-Host "[4/5] Toutes les dependances sont installees [OK]" -ForegroundColor Green
}

Pop-Location

# ========================================
# 5. Configuration CMake
# ========================================
Write-Host ""
Write-Host "[5/5] Configuration du projet..." -ForegroundColor Yellow

$projectPath = "C:\Users\Administrator\git_clone\surveillance-system\vision-service"
$buildPath = "$projectPath\build-windows"

if (Test-Path $projectPath) {
    Write-Host "  -> Creation dossier build..." -ForegroundColor White
    
    if (Test-Path $buildPath) {
        Write-Host "    Nettoyage ancien build..." -ForegroundColor Gray
        Remove-Item $buildPath -Recurse -Force
    }
    
    New-Item -ItemType Directory -Path $buildPath -Force | Out-Null
    
    Write-Host "  -> Configuration CMake..." -ForegroundColor White
    Push-Location $buildPath
    
    cmake .. -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_TOOLCHAIN_FILE="$vcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
        -DCMAKE_BUILD_TYPE=Release
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  [OK] CMake configure" -ForegroundColor Green
    } else {
        Write-Host "  [X] Erreur configuration CMake" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    
    Pop-Location
} else {
    Write-Host "  [X] Projet non trouve: $projectPath" -ForegroundColor Red
    exit 1
}

# ========================================
# Résumé
# ========================================
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  [SUCCESS] Setup termine avec succes!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Prochaines étapes:" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Compiler vision-service:" -ForegroundColor White
Write-Host "   cd $buildPath" -ForegroundColor Gray
Write-Host "   cmake --build . --config Release -j 8" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Exécuter:" -ForegroundColor White
Write-Host "   .\Release\vision-service.exe --port=50051" -ForegroundColor Gray
Write-Host ""
Write-Host "3. Tester RTSP avec vraie caméra:" -ForegroundColor White
Write-Host "   Ajouter URL dans http://localhost:8080" -ForegroundColor Gray
Write-Host ""

# Demander si l'utilisateur veut compiler maintenant
Write-Host ""
$compile = Read-Host "Compiler vision-service maintenant? (O/N)"

if ($compile -eq "O") {
    Write-Host ""
    Write-Host "Compilation en cours..." -ForegroundColor Yellow
    
    Push-Location $buildPath
    cmake --build . --config Release -j 8
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "[SUCCESS] Compilation reussie!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Binaire: $buildPath\Release\vision-service.exe" -ForegroundColor Cyan
    } else {
        Write-Host ""
        Write-Host "[X] Erreur de compilation" -ForegroundColor Red
    }
    
    Pop-Location
}

Write-Host ""
Write-Host "Documentation complète: WINDOWS_BUILD_SETUP.md" -ForegroundColor Cyan

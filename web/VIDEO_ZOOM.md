# 🔍 Zoom Numérique Temps Réel

## Vue d'ensemble

Le système de zoom numérique permet d'agrandir et de déplacer les flux vidéo en direct pour examiner des détails spécifiques. Cette fonctionnalité est essentielle pour identifier des personnes, lire des plaques d'immatriculation, ou analyser des zones d'intérêt sans perdre la vue d'ensemble.

## ✨ Fonctionnalités

### 1. **Zoom Molette** 🖱️
- Zoom avec la molette de la souris (2x à 8x)
- Zoom progressif par pas de 0.2x
- Zoom centré sur la position du curseur
- Animation fluide avec transition CSS

### 2. **Pan (Déplacement)** ✋
- Click-drag pour déplacer la vue zoomée
- Curseur "grab" quand zoomé
- Limites automatiques (empêche de sortir de l'image)
- Navigation au clavier (flèches directionnelles)

### 3. **Réinitialisation** 🔄
- Double-clic sur la vidéo pour reset
- Bouton de reset visible quand zoomé
- Raccourci clavier : touche `0`
- Animation de retour progressive

### 4. **Contrôles Visuels** 🎮
- Indicateur de niveau de zoom (ex: "2.5×")
- Boutons +/− pour zoom incrémental
- Bouton reset (⟲) en haut à gauche
- Tooltips avec raccourcis clavier

### 5. **Picture-in-Picture** 🖼️
- Mode PiP pour surveillance tout en travaillant
- Bouton PiP dans l'overlay de la caméra
- Support navigateurs modernes (Chrome, Edge, Safari)
- Toast notifications pour feedback

### 6. **Raccourcis Clavier** ⌨️
- `+` ou `=` : Zoom avant (+0.5x)
- `-` : Zoom arrière (-0.5x)
- `0` : Réinitialiser le zoom
- `↑↓←→` : Déplacer la vue (quand zoomé)

## 🎯 Cas d'Usage

### Cas 1 : Identification de Personne
```
Situation : Suspect dans une zone commerciale
1. Zoomer avec molette sur le visage (4x)
2. Pan pour centrer le visage
3. Prendre snapshot avec annotations
4. Réinitialiser pour vue d'ensemble
```

### Cas 2 : Lecture de Plaque d'Immatriculation
```
Situation : Véhicule sur parking
1. Zoomer sur la plaque (6x-8x)
2. Ajuster position avec pan
3. Attendre frame nette
4. Snapshot pour archivage
```

### Cas 3 : Surveillance Multi-Tâches
```
Situation : Surveillance tout en travaillant
1. Activer PiP sur caméra critique
2. Fenêtre vidéo flottante
3. Continuer travail sur autre application
4. Zoom dans PiP si nécessaire
```

### Cas 4 : Formation / Briefing
```
Situation : Montrer détail à collègue
1. Zoomer sur zone d'intérêt
2. Utiliser flèches pour scanner la zone
3. Screenshot pour rapport
4. Reset et passer à caméra suivante
```

## 📖 Guide d'Utilisation

### Zoomer sur une Vidéo

**Méthode 1 : Molette de Souris** (Recommandé)
1. Survoler la vidéo avec le curseur
2. Faire défiler la molette vers le haut pour zoomer
3. Faire défiler vers le bas pour dézoomer
4. Le zoom se centre automatiquement sur le curseur

**Méthode 2 : Boutons +/−**
1. Survoler la caméra
2. Cliquer sur le bouton `+` (bas à droite) pour zoomer
3. Cliquer sur le bouton `−` pour dézoomer

**Méthode 3 : Raccourcis Clavier**
```javascript
// Focus sur la caméra (survol)
Appuyer sur + ou = : Zoom avant
Appuyer sur - : Zoom arrière
```

### Déplacer la Vue Zoomée (Pan)

**Méthode 1 : Click-Drag** (Recommandé)
1. Zoomer sur la vidéo (>1.0×)
2. Cliquer et maintenir sur la vidéo
3. Déplacer le curseur pour pan
4. Relâcher pour arrêter

**Méthode 2 : Clavier**
```
↑ : Déplacer vers le haut
↓ : Déplacer vers le bas
← : Déplacer vers la gauche
→ : Déplacer vers la droite
```

### Réinitialiser le Zoom

**Méthode 1 : Double-Clic**
- Double-cliquer sur la vidéo

**Méthode 2 : Bouton Reset**
- Cliquer sur le bouton ⟲ (haut à gauche)

**Méthode 3 : Clavier**
- Appuyer sur la touche `0`

### Activer Picture-in-Picture

1. Caméra doit être en streaming (active)
2. Cliquer sur le bouton 🖼️ dans l'overlay
3. Fenêtre vidéo flottante apparaît
4. Cliquer à nouveau pour désactiver

**Navigation PiP** :
- Redimensionner la fenêtre PiP (coins)
- Déplacer la fenêtre PiP (drag)
- Contrôles de lecture (pause/play)

## 🔧 Configuration Technique

### Limites de Zoom

```javascript
// Configuration dans VideoZoomController
const MIN_ZOOM = 1.0;   // Zoom minimum (vue normale)
const MAX_ZOOM = 8.0;   // Zoom maximum
const ZOOM_STEP = 0.2;  // Pas pour molette
const KEYBOARD_STEP = 0.5; // Pas pour +/−
```

### Performance

```javascript
// Optimisations appliquées
- CSS transform (hardware-accelerated)
- will-change: transform (GPU)
- Transitions désactivées pendant drag
- Clamping des valeurs de pan
```

### Structure de Données

```javascript
// État du zoom par caméra
{
    videoElement: HTMLVideoElement,
    container: HTMLElement,
    zoomLevel: 1.0,        // 1.0 à 8.0
    panX: 0,               // Position X en px
    panY: 0,               // Position Y en px
    isDragging: false,     // État du drag
    dragStartX: 0,         // Point de départ X
    dragStartY: 0,         // Point de départ Y
    panStartX: 0,          // Pan initial X
    panStartY: 0           // Pan initial Y
}
```

### API JavaScript

```javascript
// Instance globale
window.videoZoom = new VideoZoomController();

// Initialiser le zoom pour une caméra
videoZoom.init(cameraId, videoElement);

// Méthodes de zoom
videoZoom.zoomIn(cameraId);           // +0.5×
videoZoom.zoomOut(cameraId);          // -0.5×
videoZoom.reset(cameraId);            // Retour à 1.0×
videoZoom.setZoomLevel(cameraId, 3.0); // Zoom spécifique

// Récupérer le niveau actuel
const level = videoZoom.getZoomLevel(cameraId);

// Zoomer sur une région spécifique (x, y, width, height en px)
videoZoom.zoomToRegion(cameraId, 100, 100, 200, 150);

// Picture-in-Picture
await videoZoom.togglePiP(cameraId);

// Nettoyer (quand caméra arrêtée)
videoZoom.destroy(cameraId);
```

### Événements

```javascript
// Pas d'événements custom pour l'instant
// Utiliser les événements natifs si nécessaire

// Écouter le changement de zoom (future version)
// window.addEventListener('zoom-changed', (e) => {
//     console.log('Nouveau zoom:', e.detail.zoomLevel);
// });
```

## 🎨 Interface Utilisateur

### Éléments Visuels

```
┌─────────────────────────────────────┐
│ [⟲]                          [2.5×] │ ← Reset + Indicateur
│                                     │
│                                     │
│         [Vidéo Zoomée]              │
│                                     │
│                                     │
│                                     │
│ 🖼️                       [+]       │ ← PiP + Contrôles
│                          [−]       │
│                                     │
│ 📷  ⛶                               │ ← Actions normales
└─────────────────────────────────────┘
```

### États du Curseur

- **Défaut** (`default`) : Vue normale (1.0×)
- **Grab** (`grab`) : Vue zoomée, prêt à déplacer
- **Grabbing** (`grabbing`) : En train de déplacer

### Indicateurs

**Zoom Indicator** :
- Position : Haut à droite
- Format : "X.X×" (ex: "2.5×")
- Visible uniquement si zoom > 1.0×
- Fond noir semi-transparent

**Reset Button** :
- Position : Haut à gauche
- Icône : ⟲
- Visible uniquement si zoom > 1.0×
- Hover : effet scale 1.1

**Zoom Controls** :
- Position : Bas à droite (au-dessus overlay)
- Boutons circulaires +/−
- Visible au survol de la caméra
- Toujours visibles sur mobile

## 📱 Responsive Design

### Desktop (>768px)
- Contrôles visibles au survol
- Molette souris privilégiée
- PiP disponible
- Raccourcis clavier actifs

### Tablet (768px)
- Contrôles toujours visibles
- Touch gestures (pinch-to-zoom futur)
- Boutons légèrement plus petits

### Mobile (<480px)
- Contrôles permanents (pas de hover)
- Boutons adaptés au tactile (32px minimum)
- PiP masqué (non supporté mobile)
- Double-tap pour reset

## 📊 Performance

### Métriques

- **Taille du module** : ~8KB (video-zoom.js)
- **CSS additionnel** : ~3KB (styles zoom)
- **Impact CPU** : Minimal (transform GPU-accelerated)
- **Fluidité** : 60 FPS (transitions CSS)

### Optimisations Appliquées

1. **Hardware Acceleration**
   ```css
   transform: scale() translate();
   will-change: transform;
   ```

2. **Transitions Conditionnelles**
   ```javascript
   // Désactiver pendant drag pour fluidité
   state.isDragging ? 'none' : 'transform 0.2s ease-out'
   ```

3. **Clamping des Valeurs**
   ```javascript
   // Éviter calculs inutiles
   zoomLevel = Math.max(1.0, Math.min(8.0, newZoom));
   ```

4. **Calculs Optimisés**
   ```javascript
   // Calcul pan proportionnel au zoom
   const maxPan = (zoomLevel - 1) * 50;
   ```

## 🔐 Compatibilité Navigateurs

### Support Complet
- ✅ Chrome 90+ (Desktop & Android)
- ✅ Edge 90+
- ✅ Firefox 88+
- ✅ Safari 14+ (macOS & iOS)

### Picture-in-Picture
- ✅ Chrome 70+ (Desktop & Android)
- ✅ Edge 79+
- ✅ Safari 13.1+ (macOS)
- ❌ Firefox (pas supporté nativement)
- ❌ iOS Safari (limitations système)

### Fallbacks

```javascript
// Vérifier support PiP
if (!document.pictureInPictureEnabled) {
    console.log('PiP non disponible');
    // Masquer bouton PiP
}

// Vérifier support wheel event
if ('onwheel' in document) {
    // Zoom molette disponible
}
```

## 🐛 Dépannage

### Problème : Zoom ne fonctionne pas

**Solution 1** : Vérifier que le module est chargé
```javascript
console.log(window.videoZoom); // Doit être défini
```

**Solution 2** : Vérifier que la caméra est initialisée
```javascript
console.log(videoZoom.instances.has('cam-001')); // Doit être true
```

**Solution 3** : Vérifier événement wheel
```javascript
// Tester manuellement
const container = document.querySelector('[data-camera-id="cam-001"] .camera-video-container');
container.addEventListener('wheel', (e) => {
    console.log('Wheel event:', e.deltaY);
}, { passive: false });
```

### Problème : Pan ne fonctionne pas

**Solution** : Vérifier que le zoom est > 1.0×
```javascript
const level = videoZoom.getZoomLevel('cam-001');
console.log('Zoom level:', level); // Doit être > 1.0
```

### Problème : PiP ne démarre pas

**Solution 1** : Vérifier le support navigateur
```javascript
console.log('PiP enabled:', document.pictureInPictureEnabled);
```

**Solution 2** : Vérifier que la vidéo a du contenu
```javascript
const video = document.querySelector('[data-camera-id="cam-001"] video');
console.log('Video ready:', video.readyState >= 2);
```

**Solution 3** : Gestion des erreurs
```javascript
try {
    await video.requestPictureInPicture();
} catch(err) {
    console.error('PiP error:', err.name, err.message);
    // NotSupportedError, InvalidStateError, etc.
}
```

### Problème : Performance dégradée

**Solution 1** : Vérifier hardware acceleration
```javascript
// Ouvrir DevTools > Rendering > Paint flashing
// Les éléments verts utilisent GPU
```

**Solution 2** : Limiter le nombre de caméras zoomées
```javascript
// Désactiver zoom sur caméras non visibles
if (!isVisible) {
    videoZoom.reset(cameraId);
}
```

**Solution 3** : Désactiver transitions pendant animation
```css
/* Déjà implémenté automatiquement */
transition: state.isDragging ? 'none' : 'transform 0.2s';
```

## 🚀 Roadmap

### v1.1 (Actuel)
- ✅ Zoom molette (1x-8x)
- ✅ Pan click-drag
- ✅ Double-clic reset
- ✅ Raccourcis clavier
- ✅ Indicateurs visuels
- ✅ Picture-in-Picture

### v1.2 (Prochaine version)
- [ ] Touch gestures (pinch-to-zoom)
- [ ] Zoom sur région sélectionnée (rectangle)
- [ ] Historique de positions (back/forward)
- [ ] Presets de zoom (2x, 4x, 6x, 8x)
- [ ] Zoom synchronisé multi-caméras

### v2.0 (Long terme)
- [ ] Suivi automatique (auto-tracking)
- [ ] Zoom intelligent (détection visage/plaque)
- [ ] Stabilisation d'image (anti-shake)
- [ ] Amélioration d'image (sharpening)
- [ ] Export vidéo zoomée

## 🎓 Exemples Avancés

### Zoom Automatique sur Détection

```javascript
// Écouter événement de détection
window.addEventListener('motion-detected', (e) => {
    const { camera_id, region } = e.detail;
    
    // Zoomer sur la région détectée
    if (region) {
        videoZoom.zoomToRegion(
            camera_id,
            region.x,
            region.y,
            region.width,
            region.height
        );
    }
});
```

### Préréglages de Zoom

```javascript
// Définir des presets
const ZOOM_PRESETS = {
    overview: 1.0,
    medium: 2.5,
    detail: 4.0,
    extreme: 6.0
};

function applyPreset(cameraId, presetName) {
    const level = ZOOM_PRESETS[presetName];
    videoZoom.setZoomLevel(cameraId, level);
    Toast.info(`Preset: ${presetName} (${level}×)`);
}

// Utilisation
applyPreset('cam-001', 'detail');
```

### Zoom Synchronisé Multi-Caméras

```javascript
class SyncedZoomController {
    constructor(cameraIds) {
        this.cameras = cameraIds;
        this.masterCamera = null;
    }
    
    setMaster(cameraId) {
        this.masterCamera = cameraId;
        
        // Synchroniser les autres caméras
        const masterLevel = videoZoom.getZoomLevel(cameraId);
        this.cameras.forEach(id => {
            if (id !== cameraId) {
                videoZoom.setZoomLevel(id, masterLevel);
            }
        });
    }
    
    syncZoom(level) {
        this.cameras.forEach(id => {
            videoZoom.setZoomLevel(id, level);
        });
    }
}

// Utilisation
const syncZoom = new SyncedZoomController(['cam-001', 'cam-002', 'cam-003']);
syncZoom.syncZoom(3.0); // Toutes les caméras à 3×
```

### Tour Guidé Automatique

```javascript
async function autoTour(cameraId, regions, duration = 5000) {
    for (const region of regions) {
        // Zoomer sur la région
        videoZoom.zoomToRegion(
            cameraId,
            region.x,
            region.y,
            region.width,
            region.height
        );
        
        // Attendre
        await new Promise(resolve => setTimeout(resolve, duration));
    }
    
    // Reset à la fin
    videoZoom.reset(cameraId);
}

// Utilisation
const regions = [
    { x: 100, y: 100, width: 200, height: 150 }, // Entrée
    { x: 500, y: 200, width: 150, height: 150 }, // Caisse
    { x: 800, y: 300, width: 200, height: 200 }  // Sortie
];

autoTour('cam-001', regions, 3000);
```

## 📝 Notes de Développement

### Architecture

- **video-zoom.js** : Module indépendant (pas de dépendances)
- **camera-manager.js** : Initialise le zoom après setup stream
- **enhanced.css** : Styles modulaires avec responsive

### Extensibilité

Le système est conçu pour :
- Ajouter de nouveaux modes de zoom (fit, fill, etc.)
- Intégrer avec analytics (tracking zoom usage)
- Étendre avec IA (zoom intelligent)

### Tests Manuels

```javascript
// Test du zoom
videoZoom.init('test-cam', videoElement);
videoZoom.setZoomLevel('test-cam', 3.0);
console.log(videoZoom.getZoomLevel('test-cam')); // 3.0

// Test du pan
// Cliquer-glisser sur la vidéo
// Vérifier que transform est appliqué

// Test PiP
await videoZoom.togglePiP('test-cam');
// Vérifier fenêtre PiP

// Cleanup
videoZoom.destroy('test-cam');
```

### Considérations de Sécurité

- **Pas de données sensibles** : Seulement transformations CSS
- **Validation des limites** : Clamping pour éviter valeurs extrêmes
- **Pas de requêtes réseau** : Tout côté client

---

**Version** : 1.1  
**Dernière mise à jour** : Novembre 2025  
**Compatibilité** : Chrome 90+, Firefox 88+, Safari 14+, Edge 90+  
**Taille** : 8KB JS + 3KB CSS

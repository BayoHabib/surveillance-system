# 📁 Gestion des Groupes et Favoris de Caméras

## Vue d'ensemble

Le système de gestion des groupes permet d'organiser efficacement un grand nombre de caméras en créant des groupes personnalisés et en marquant des caméras favorites. Cette fonctionnalité est essentielle pour faciliter la navigation et la surveillance ciblée dans des installations avec 10+ caméras.

## ✨ Fonctionnalités

### 1. **Favoris** ⭐
- Marquer/démarquer rapidement des caméras comme favorites
- Étoile dorée visible sur chaque carte de caméra
- Filtre dédié pour afficher uniquement les favoris
- Persistance dans LocalStorage

### 2. **Groupes Personnalisés** 📂
- Créer des groupes illimités avec noms et icônes personnalisés
- Groupes par défaut : Entrées 🚪, Parking 🚗, Bureaux 💼, Extérieur 🌳
- Assigner une caméra à plusieurs groupes simultanément
- Couleurs et icônes pour identification visuelle rapide

### 3. **Sidebar de Navigation** 🗂️
- Vue compacte de tous les groupes
- Compteur de caméras par groupe
- Filtre rapide par clic
- Toggle collapse/expand (fixe sur desktop)
- Responsive : plein écran sur mobile

### 4. **Badges Visuels** 🏷️
- Badges de groupe affichés au survol de la caméra
- Limite de 2 badges visibles + compteur (+X)
- Icônes des groupes pour identification rapide

### 5. **Gestion Avancée** ⚙️
- Modal de gestion des groupes par caméra
- Checkboxes pour ajouter/retirer des groupes
- Export/Import de configuration (JSON)
- Suppression de groupes avec confirmation

## 🎯 Cas d'Usage

### Cas 1 : Grande Entreprise (50+ caméras)
```
Groupes créés :
- 🚪 Entrées (8 caméras)
- 🚗 Parking Est (12 caméras)
- 🚗 Parking Ouest (10 caméras)
- 💼 Bureaux (15 caméras)
- 🏭 Production (8 caméras)
- 🌳 Périmètre (20 caméras)

Workflow :
1. Arrivée le matin → Clic sur "Entrées" pour voir toutes les entrées
2. Incident parking → Clic sur "Parking Est", layout 3×3
3. Surveillance nocturne → Clic sur "Périmètre", marquer 3 caméras en favoris
```

### Cas 2 : Résidence Privée (10 caméras)
```
Groupes :
- 🏠 Maison principale (4 caméras)
- 🏡 Jardin (3 caméras)
- 🚗 Garage (2 caméras)
- 🚪 Portail (1 caméra)

Favoris :
- Caméra entrée principale
- Caméra allée
```

### Cas 3 : Commerce (15 caméras)
```
Groupes :
- 🏪 Zone clientèle (6 caméras)
- 📦 Entrepôt (4 caméras)
- 🚪 Accès personnel (3 caméras)
- 🚗 Parking (2 caméras)
```

## 📖 Guide d'Utilisation

### Marquer une Caméra Favorite

1. **Méthode Visuelle** :
   - Survoler la carte de la caméra
   - Cliquer sur l'étoile (⭐) en haut à droite
   - L'étoile devient dorée quand la caméra est favorite

2. **Via Code** :
   ```javascript
   cameraGroups.toggleFavorite('cam-001');
   // ou
   cameraGroups.isFavorite('cam-001'); // Check status
   ```

### Créer un Groupe

1. **Via Interface** :
   - Cliquer sur le bouton **+** à côté de "📁 Groupes" dans la sidebar
   - Entrer le nom du groupe (ex: "Parking")
   - Choisir un icône parmi les proposés : 📹🚪🚗💼🌳🏠🏢🏪🎯🔒
   - Le groupe est créé instantanément

2. **Via Code** :
   ```javascript
   cameraGroups.createGroup('parking', 'Parking', '🚗');
   ```

### Assigner une Caméra à un Groupe

1. **Méthode Visuelle** :
   - Cliquer sur le bouton **"📁 Groupes"** dans les actions de la caméra
   - Cocher/décocher les groupes souhaités
   - Fermer le modal (sauvegarde automatique)

2. **Via Code** :
   ```javascript
   cameraGroups.addToGroup('cam-001', 'parking');
   cameraGroups.removeFromGroup('cam-001', 'parking');
   ```

### Filtrer par Groupe

1. **Via Sidebar** :
   - Cliquer sur un groupe dans la sidebar
   - Seules les caméras du groupe sont affichées
   - Le compteur de caméras est mis à jour

2. **Via Code** :
   ```javascript
   cameraGroups.filterByGroup('parking');
   // ou
   cameraGroups.filterByGroup('favorites');
   cameraGroups.filterByGroup('all'); // Reset
   ```

### Supprimer un Groupe

1. **Via Interface** :
   - Survoler le groupe dans la sidebar
   - Cliquer sur le bouton **×** rouge qui apparaît
   - Confirmer la suppression

2. **Via Code** :
   ```javascript
   cameraGroups.deleteGroup('parking');
   ```

## 🔧 Configuration Technique

### Structure LocalStorage

```javascript
// Favoris
localStorage.camera_favorites = ["cam-001", "cam-003", "cam-005"]

// Groupes
localStorage.camera_groups = [
    ["entries", {
        name: "Entrées",
        icon: "🚪",
        cameras: ["cam-001", "cam-002"]
    }],
    ["parking", {
        name: "Parking",
        icon: "🚗",
        cameras: ["cam-003", "cam-004", "cam-005"]
    }]
]
```

### API JavaScript

```javascript
// Instance globale
window.cameraGroups = new CameraGroupsManager();

// Méthodes principales
cameraGroups.toggleFavorite(cameraId)
cameraGroups.isFavorite(cameraId)
cameraGroups.getFavorites()

cameraGroups.createGroup(groupId, name, icon)
cameraGroups.deleteGroup(groupId)
cameraGroups.getAllGroups()

cameraGroups.addToGroup(cameraId, groupId)
cameraGroups.removeFromGroup(cameraId, groupId)
cameraGroups.getGroupCameras(groupId)

cameraGroups.filterByGroup(groupId)
cameraGroups.getCurrentFilter()

cameraGroups.exportConfig()
cameraGroups.importConfig(file)
```

### Événements

```javascript
// Écouter les changements de favoris
window.addEventListener('favorites-changed', (e) => {
    console.log('Favoris mis à jour:', e.detail.favorites);
});

// Écouter les changements de filtre de groupe
window.addEventListener('group-filter-changed', (e) => {
    console.log('Filtre actif:', e.detail.groupId);
});
```

## 🎨 Interface Utilisateur

### Sidebar

```
┌─────────────────────────────────┐
│ 📋 FILTRES                      │
│ ┌─────────────────────────────┐ │
│ │ 📹 Toutes            [12]   │ │
│ │ ⭐ Favoris          [3]    │ │
│ └─────────────────────────────┘ │
│                                 │
│ 📁 GROUPES              [+]    │
│ ┌─────────────────────────────┐ │
│ │ 🚪 Entrées          [2] [×]│ │
│ │ 🚗 Parking          [4] [×]│ │
│ │ 💼 Bureaux          [3] [×]│ │
│ │ 🌳 Extérieur        [3] [×]│ │
│ └─────────────────────────────┘ │
└─────────────────────────────────┘
```

### Carte de Caméra

```
┌───────────────────────────────┐
│ 🏷️ Parking  Entrées    [⭐]  │ ← Badges + Favori
│                               │
│     [Vidéo en direct]         │
│                               │
│ 📷  ⛶                         │ ← Actions
│───────────────────────────────│
│ Caméra 01        🟢 En ligne │
│ 📍 Parking      🎞️ 30 FPS   │
│                               │
│ [⏹ Arrêter] [⚙️] [📁 Groupes]│
└───────────────────────────────┘
```

### Modal de Gestion

```
┌───────────────────────────────┐
│ 📁 Gérer les groupes          │
│    Caméra 01                  │
│                               │
│ ☑ 🚪 Entrées         2 caméras│
│ ☑ 🚗 Parking         4 caméras│
│ ☐ 💼 Bureaux         3 caméras│
│ ☐ 🌳 Extérieur       3 caméras│
│                               │
│     [Fermer]                  │
└───────────────────────────────┘
```

## 🎯 Responsive Design

### Desktop (>768px)
- Sidebar fixe à gauche (260px)
- Toggle pour masquer/afficher
- Main content avec margin-left: 260px
- Badges visibles au survol

### Tablet (768px)
- Sidebar overlay plein écran
- Fermeture automatique après sélection
- Bouton toggle visible en permanence

### Mobile (<480px)
- Sidebar plein écran avec backdrop
- Fermeture au clic en dehors
- Actions de groupe via modal
- Favoris toujours visibles (pas de hover)

## 📊 Performance

### Métriques
- **Taille du module** : ~12KB (camera-groups.js)
- **CSS additionnel** : ~4KB (styles sidebar + badges)
- **LocalStorage** : ~5KB pour 100 caméras + 20 groupes
- **Render time** : <5ms pour 50 caméras avec groupes

### Optimisations
- Filtrage côté client (pas de requêtes serveur)
- Rendu différentiel (seulement les caméras filtrées)
- Événements délégués pour les clics
- Debounce sur les mises à jour de compteurs

## 🔐 Sécurité & Confidentialité

### LocalStorage
- **Avantages** :
  - Pas de données sensibles (seulement IDs et noms)
  - Persistance locale par navigateur
  - Pas de requêtes réseau supplémentaires
  
- **Limitations** :
  - Configuration non partagée entre utilisateurs
  - Perdue si cache navigateur effacé
  - Non synchronisée entre devices

### Future : Backend Storage (Roadmap)
```go
// Endpoints futurs
POST /api/v1/users/:id/groups
GET  /api/v1/users/:id/groups
PUT  /api/v1/users/:id/groups/:groupId
```

## 🚀 Import/Export de Configuration

### Exporter

```javascript
// Via code
cameraGroups.exportConfig();

// Télécharge un fichier JSON :
{
    "favorites": ["cam-001", "cam-003"],
    "groups": [
        {
            "id": "parking",
            "name": "Parking",
            "icon": "🚗",
            "cameras": ["cam-003", "cam-004"]
        }
    ],
    "exportDate": "2024-01-15T10:30:00.000Z",
    "version": "1.0"
}
```

### Importer

```javascript
// Via code
const fileInput = document.createElement('input');
fileInput.type = 'file';
fileInput.accept = '.json';
fileInput.onchange = (e) => {
    cameraGroups.importConfig(e.target.files[0]);
};
fileInput.click();
```

### Cas d'Usage Import/Export
1. **Backup** : Sauvegarder configuration avant maintenance
2. **Migration** : Transférer config entre machines
3. **Templates** : Partager configurations types (bureau, commerce, résidence)
4. **Recovery** : Restaurer après effacement cache

## 🐛 Dépannage

### Problème : Sidebar ne s'affiche pas

**Solution** :
```javascript
// Vérifier que le module est chargé
console.log(window.cameraGroups); // Doit être défini

// Vérifier l'élément DOM
console.log(document.getElementById('groupsSidebar')); // Doit exister
```

### Problème : Favoris ne persistent pas

**Solution** :
```javascript
// Vérifier LocalStorage
console.log(localStorage.getItem('camera_favorites'));

// Vérifier quota (5MB limite)
try {
    localStorage.setItem('test', 'test');
    localStorage.removeItem('test');
    console.log('LocalStorage disponible');
} catch(e) {
    console.error('LocalStorage plein ou bloqué', e);
}
```

### Problème : Filtrage ne fonctionne pas

**Solution** :
```javascript
// Vérifier le filtre actif
console.log(cameraGroups.getCurrentFilter());

// Forcer un re-filtrage
cameraManager.applyFilters();

// Vérifier les événements
window.addEventListener('group-filter-changed', e => {
    console.log('Filtre changé:', e.detail);
});
```

## 📈 Roadmap

### v1.1 (Actuel)
- ✅ Favoris avec étoile
- ✅ Groupes personnalisés
- ✅ Sidebar de navigation
- ✅ Badges visuels
- ✅ Export/Import JSON

### v1.2 (Prochaine version)
- [ ] Drag & Drop pour assigner caméras
- [ ] Groupes imbriqués (sous-groupes)
- [ ] Recherche de caméras dans sidebar
- [ ] Ordre personnalisé des groupes
- [ ] Couleurs personnalisées par groupe

### v2.0 (Long terme)
- [ ] Backend storage (synchronisation multi-device)
- [ ] Partage de groupes entre utilisateurs
- [ ] Templates de groupes pré-configurés
- [ ] Groupes intelligents (auto-assignment par critères)
- [ ] Statistiques par groupe (temps de surveillance, alertes)

## 🎓 Exemples Avancés

### Créer un Groupe pour Caméras Extérieures Automatiquement

```javascript
// Filtrer toutes les caméras avec "outdoor" dans le nom
const outdoorCameras = cameraManager.cameras
    .filter(c => c.location.toLowerCase().includes('outdoor'))
    .map(c => c.id);

// Créer le groupe
cameraGroups.createGroup('outdoor-auto', 'Extérieur', '🌳');

// Assigner toutes les caméras
outdoorCameras.forEach(camId => {
    cameraGroups.addToGroup(camId, 'outdoor-auto');
});
```

### Système d'Alertes par Groupe

```javascript
// Écouter les alertes
window.addEventListener('intrusion-detected', (e) => {
    const cameraId = e.detail.camera_id;
    
    // Trouver les groupes de cette caméra
    const groups = cameraGroups.getAllGroups()
        .filter(g => g.cameras.includes(cameraId));
    
    // Notifier par groupe
    groups.forEach(group => {
        Toast.error(`Intrusion détectée - ${group.name}`, '', 0);
    });
});
```

### Statistiques par Groupe

```javascript
function getGroupStats(groupId) {
    const cameras = cameraGroups.getGroupCameras(groupId);
    const cameraData = cameraManager.cameras.filter(c => cameras.includes(c.id));
    
    return {
        total: cameras.length,
        online: cameraData.filter(c => c.status === 'streaming').length,
        offline: cameraData.filter(c => c.status === 'offline').length,
        avgFps: cameraData.reduce((sum, c) => sum + (c.fps || 0), 0) / cameras.length
    };
}

// Utilisation
const parkingStats = getGroupStats('parking');
console.log(`Parking: ${parkingStats.online}/${parkingStats.total} en ligne`);
```

## 📝 Notes de Développement

### Architecture
- **camera-groups.js** : Module indépendant (pas de dépendances externes)
- **camera-manager.js** : Intégration via événements (loose coupling)
- **enhanced.css** : Styles modulaires et responsive

### Extensibilité
Le système est conçu pour être facilement étendu :
- Ajout de nouveaux types de filtres
- Intégration avec d'autres modules (analytics, alerts)
- Migration vers backend storage sans refonte UI

### Tests
```javascript
// Test manual
cameraGroups.createGroup('test', 'Test Group', '🧪');
cameraGroups.addToGroup('cam-001', 'test');
cameraGroups.toggleFavorite('cam-001');
cameraGroups.filterByGroup('test');
// Vérifier UI
cameraGroups.deleteGroup('test');
```

---

**Version** : 1.1  
**Dernière mise à jour** : Janvier 2025  
**Compatibilité** : Chrome 90+, Firefox 88+, Safari 14+, Edge 90+

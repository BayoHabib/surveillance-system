# 📸 Snapshot Editor - Documentation

## Vue d'ensemble

L'éditeur de captures avancé permet de capturer des images depuis les flux vidéo des caméras et d'y ajouter des annotations professionnelles avant export.

## ✨ Fonctionnalités Principales

### 🛠️ Outils d'Annotation

1. **➡️ Flèche** - Pointer des éléments spécifiques
   - Click-drag pour dessiner
   - Tête de flèche automatique
   - Personnalisable (couleur, épaisseur)

2. **⬜ Rectangle** - Encadrer des zones
   - Click-drag pour définir la zone
   - Contour personnalisable
   - Idéal pour mettre en évidence

3. **⭕ Cercle** - Entourer des objets
   - Click-drag pour définir le rayon
   - Centre fixe, rayon dynamique
   - Parfait pour zones circulaires

4. **📝 Texte** - Ajouter des commentaires
   - Zone de texte dédiée dans la sidebar
   - Taille de police ajustable (12-72px)
   - Aperçu en temps réel
   - Click pour placer le texte

5. **✏️ Dessin Libre** - Dessiner à main levée
   - Suivi du mouvement de la souris
   - Lignes lissées automatiquement
   - Idéal pour annotations complexes

6. **🔒 Floutage** - Masquer zones sensibles
   - Pixelisation automatique
   - Protection données confidentielles
   - Conforme RGPD

### 🎨 Personnalisation

#### Couleurs Disponibles
- 🔴 Rouge (#ff0000)
- 🟢 Vert (#00ff00)
- 🔵 Bleu (#0000ff)
- 🟡 Jaune (#ffff00)
- 🟣 Magenta (#ff00ff)
- 🔷 Cyan (#00ffff)
- ⚪ Blanc (#ffffff)
- ⚫ Noir (#000000)

#### Épaisseur
- Slider 1-10 pixels
- Aperçu visuel en temps réel
- Adapté au type d'annotation

#### Taille de Police
- Slider 12-72 pixels
- Aperçu du texte en direct
- Lisibilité optimale

### ℹ️ Métadonnées Automatiques

#### Options Activables
- **📅 Horodatage** - Date et heure de capture
- **📹 Nom Caméra** - Identification de la source
- **💧 Filigrane** - "SURVEILLANCE SYSTEM" (transparence 30%)

#### Positionnement
- Horodatage : Haut gauche
- Nom caméra : Haut gauche (sous horodatage)
- Filigrane : Bas droite

#### Style
- Texte blanc avec contour noir
- Lisibilité garantie sur tous fonds
- Police Arial, taille 16px

### 💾 Export

#### Formats Supportés

**PNG (Recommandé)**
- ✅ Qualité maximale
- ✅ Compression sans perte
- ✅ Support transparence
- ⚠️ Taille fichier plus importante
- 🎯 Usage : Preuves légales, documentation officielle

**JPG (Optimisé)**
- ✅ Taille fichier réduite (~70% vs PNG)
- ✅ Compatible universel
- ⚠️ Compression avec perte
- ⚠️ Pas de transparence
- 🎯 Usage : Partage email, archivage

#### Nomenclature Fichiers
```
snapshot-{camera_id}-{timestamp}.{format}
```

Exemple : `snapshot-cam-001-1700332156789.png`

### 📋 Gestion Annotations

#### Liste des Annotations
- Affichage en temps réel
- Compteur d'annotations
- Icône par type
- Aperçu texte (20 premiers caractères)

#### Actions Disponibles
- **↶ Annuler** - Supprimer dernière annotation
- **🗑️ Supprimer** - Supprimer annotation spécifique
- **🗑️ Effacer tout** - Supprimer toutes les annotations (confirmation requise)

## 🚀 Utilisation

### Workflow Standard

1. **Capture**
   ```javascript
   // Dans camera-manager.js
   cameraManager.takeSnapshot('camera-001');
   ```

2. **Annotation**
   - Sélectionner un outil
   - Choisir couleur/épaisseur
   - Dessiner sur l'image
   - Ajouter texte si nécessaire

3. **Configuration**
   - Activer/désactiver métadonnées
   - Ajuster annotations si besoin
   - Vérifier aperçu final

4. **Export**
   - Choisir format (PNG/JPG)
   - Téléchargement automatique
   - Fichier nommé avec timestamp

### Intégration dans Applications

```html
<!-- Inclure CSS -->
<link rel="stylesheet" href="/static/css/snapshot-editor.css">

<!-- Inclure JS -->
<script src="/static/js/snapshot-editor.js"></script>

<!-- Utiliser -->
<script>
  // Ouvrir l'éditeur
  snapshotEditor.open(videoElement, {
    id: 'camera-001',
    name: 'Entrée Principale',
    location: 'Hall'
  });
</script>
```

## 🎯 Cas d'Usage

### 1. Documentation Incident
```
Scénario : Intrusion détectée

Actions :
1. Capturer l'image du suspect
2. Entourer (cercle rouge) le visage
3. Ajouter flèche vers plaque véhicule
4. Ajouter texte : "Suspect #1 - 21h35"
5. Exporter PNG pour rapport police
```

### 2. Maintenance Préventive
```
Scénario : Équipement défectueux visible

Actions :
1. Capturer zone problématique
2. Rectangle rouge autour équipement
3. Texte descriptif : "Lampe H3 grillée"
4. Horodatage actif
5. Export JPG pour ticket maintenance
```

### 3. Preuve Légale
```
Scenario : Vol de véhicule

Actions :
1. Capturer moment critique
2. Plusieurs flèches sur détails (plaque, suspect, direction)
3. Annotations texte avec horaires
4. Activer TOUTES les métadonnées
5. Export PNG haute qualité
6. Archivage sécurisé
```

### 4. Formation Personnel
```
Scénario : Guide utilisation caméras

Actions :
1. Capturer interface caméra
2. Annotations texte pour étapes
3. Flèches vers boutons importants
4. Rectangles pour zones d'intérêt
5. Export JPG pour manuel
```

## 🎨 Interface Utilisateur

### Layout

```
┌─────────────────────────────────────────────────────────┐
│ 📸 Éditeur de Capture                              [×]  │
├─────────────────────────────────────┬───────────────────┤
│                                     │ 🛠️ Outils         │
│                                     │ ➡️🟥⭕📝✏️🔒      │
│         CANVAS                      │                   │
│      (Image + Annotations)          │ 🎨 Couleur        │
│                                     │ ●●●●●●●●          │
│                                     │                   │
│                                     │ 📏 Épaisseur      │
│                                     │ ═══ [3px]         │
│                                     │                   │
│                                     │ ℹ️ Informations   │
│                                     │ ☑ Horodatage      │
│                                     │ ☑ Nom caméra      │
│                                     │ ☑ Filigrane       │
│                                     │                   │
│                                     │ 📋 Annotations(3) │
│                                     │ ➡️ Flèche 🗑️     │
│                                     │ 📝 Texte... 🗑️   │
├─────────────────────────────────────┴───────────────────┤
│ [🗑️ Effacer] [↶ Annuler] [💾 PNG] [📥 JPG]            │
└─────────────────────────────────────────────────────────┘
```

### Responsive

**Desktop** (>768px)
- Layout horizontal (canvas | sidebar)
- Sidebar fixe 300px
- Canvas flexible

**Mobile** (<768px)
- Layout vertical (canvas / sidebar)
- Sidebar collapsible
- Canvas pleine largeur
- Outils en grille 4 colonnes

## ⚙️ Configuration Technique

### Dépendances
- **Aucune** - Vanilla JavaScript
- Canvas API natif
- Pas de bibliothèque externe

### Performances

**Taille Fichiers**
- snapshot-editor.css : ~6 KB
- snapshot-editor.js : ~15 KB
- **Total : 21 KB** (non minifié)

**Compatibilité**
- ✅ Chrome 90+
- ✅ Firefox 88+
- ✅ Safari 14+
- ✅ Edge 90+

**Limites**
- Résolution max : Limitée par RAM navigateur
- Annotations : Illimitées (stockage mémoire)
- Taille export : Dépend résolution source

### API JavaScript

```javascript
class SnapshotEditor {
  // Ouvrir l'éditeur
  open(videoElement, cameraInfo)
  
  // Fermer l'éditeur
  close()
  
  // Sélectionner outil
  selectTool(toolName)
  
  // Définir couleur
  setColor(hexColor)
  
  // Ajouter annotation
  addTextAnnotation(x, y)
  addArrowAnnotation(x1, y1, x2, y2)
  addRectAnnotation(x1, y1, x2, y2)
  addCircleAnnotation(x1, y1, x2, y2)
  addBlurAnnotation(x1, y1, x2, y2)
  
  // Gestion annotations
  deleteAnnotation(index)
  undo()
  clear()
  
  // Export
  download(format) // 'png' | 'jpg'
  
  // Rendu
  redraw()
}
```

## 🔒 Sécurité & Confidentialité

### Protection Données

1. **Traitement Local**
   - Aucune donnée envoyée au serveur
   - Traitement 100% navigateur
   - Pas de stockage cloud

2. **Outil Floutage**
   - Pixelisation irréversible
   - Conformité RGPD
   - Masquage visages/plaques

3. **Export Contrôlé**
   - Téléchargement local uniquement
   - Pas de partage automatique
   - Utilisateur garde contrôle total

### Bonnes Pratiques

✅ **À FAIRE**
- Flouter visages/plaques si partage public
- Activer horodatage pour preuves légales
- Conserver originaux non annotés
- Documenter chaîne de traçabilité

❌ **À ÉVITER**
- Annotations offensantes/discriminatoires
- Diffusion sans consentement
- Modification trompeuse de preuves
- Suppression métadonnées légales

## 🐛 Dépannage

### Problème : Éditeur ne s'ouvre pas
**Solutions :**
- Vérifier console navigateur
- S'assurer `snapshotEditor` est défini globalement
- Vérifier que videoElement a bien des dimensions

### Problème : Annotations ne s'affichent pas
**Solutions :**
- Vérifier qu'un outil est sélectionné
- S'assurer que le canvas est bien initialisé
- Tester avec autre navigateur

### Problème : Export ne fonctionne pas
**Solutions :**
- Vérifier autorisations téléchargement navigateur
- Désactiver bloqueurs popup
- Tester format alternatif (PNG vs JPG)

### Problème : Performances lentes
**Solutions :**
- Réduire nombre d'annotations
- Diminuer résolution source
- Fermer autres onglets gourmands

## 📊 Statistiques d'Utilisation

### Métriques Suivies (Futur)
- Nombre de captures par jour
- Outil le plus utilisé
- Format export préféré
- Temps moyen d'annotation
- Nombre d'annotations par capture

### Analytics (Proposition)
```javascript
// Événements à tracker
- snapshot.opened
- tool.selected
- annotation.added
- export.downloaded
- editor.closed
```

## 🚀 Roadmap Futures Améliorations

### Version 1.1 (Court terme)
- [ ] Raccourcis clavier (Ctrl+Z, Delete, ESC)
- [ ] Drag-and-drop pour repositionner annotations
- [ ] Copier/coller annotations
- [ ] Templates d'annotations sauvegardés

### Version 1.2 (Moyen terme)
- [ ] Export PDF multi-pages
- [ ] Formes géométriques additionnelles (triangle, polygone)
- [ ] Styles de traits (pointillés, tirets)
- [ ] Historique complet avec redo

### Version 2.0 (Long terme)
- [ ] Annotations collaboratives temps réel
- [ ] OCR intégré (lecture texte sur image)
- [ ] Détection automatique objets à annoter
- [ ] Export vers cloud (optionnel)
- [ ] Signature numérique des captures

## 📞 Support

Pour toute question ou problème :
- 📧 Email : support@surveillance-system.com
- 📚 Documentation : `/docs/snapshot-editor`
- 🐛 Issues : GitHub repository
- 💬 Chat : Support intégré dans l'application

---

**Version :** 1.0.0  
**Date :** Novembre 2025  
**Auteur :** Surveillance System Team  
**License :** Propriétaire

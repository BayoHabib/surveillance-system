/**
 * Camera Groups & Favorites Manager
 * Organize cameras into custom groups and mark favorites
 */

class CameraGroupsManager {
    constructor() {
        this.favorites = new Set();
        this.groups = new Map();
        this.currentGroup = 'all';
        
        this.loadFromStorage();
        this.init();
    }

    init() {
        // Will be called after DOM is ready
    }

    /**
     * Load favorites and groups from LocalStorage
     */
    loadFromStorage() {
        try {
            // Load favorites
            const savedFavorites = localStorage.getItem('camera_favorites');
            if (savedFavorites) {
                this.favorites = new Set(JSON.parse(savedFavorites));
            }

            // Load groups
            const savedGroups = localStorage.getItem('camera_groups');
            if (savedGroups) {
                const groupsArray = JSON.parse(savedGroups);
                this.groups = new Map(groupsArray);
            } else {
                // Default groups
                this.groups.set('entries', { name: 'Entrées', icon: '🚪', cameras: [] });
                this.groups.set('parking', { name: 'Parking', icon: '🚗', cameras: [] });
                this.groups.set('offices', { name: 'Bureaux', icon: '💼', cameras: [] });
                this.groups.set('outdoor', { name: 'Extérieur', icon: '🌳', cameras: [] });
            }
        } catch (error) {
            console.error('Failed to load camera groups:', error);
        }
    }

    /**
     * Save to LocalStorage
     */
    saveToStorage() {
        try {
            // Save favorites
            localStorage.setItem('camera_favorites', JSON.stringify([...this.favorites]));

            // Save groups
            const groupsArray = Array.from(this.groups.entries());
            localStorage.setItem('camera_groups', JSON.stringify(groupsArray));
        } catch (error) {
            console.error('Failed to save camera groups:', error);
        }
    }

    /**
     * Toggle camera favorite status
     */
    toggleFavorite(cameraId) {
        if (this.favorites.has(cameraId)) {
            this.favorites.delete(cameraId);
            Toast.info('Retiré des favoris');
        } else {
            this.favorites.add(cameraId);
            Toast.success('Ajouté aux favoris');
        }
        
        this.saveToStorage();
        this.updateUI();
        
        // Dispatch event for camera manager
        window.dispatchEvent(new CustomEvent('favorites-changed', { 
            detail: { favorites: [...this.favorites] }
        }));
    }

    /**
     * Check if camera is favorite
     */
    isFavorite(cameraId) {
        return this.favorites.has(cameraId);
    }

    /**
     * Get all favorites
     */
    getFavorites() {
        return [...this.favorites];
    }

    /**
     * Create new group
     */
    createGroup(groupId, name, icon = '📹') {
        if (this.groups.has(groupId)) {
            Toast.warning('Ce groupe existe déjà');
            return false;
        }

        this.groups.set(groupId, {
            name,
            icon,
            cameras: []
        });

        this.saveToStorage();
        this.updateUI();
        Toast.success(`Groupe "${name}" créé`);
        return true;
    }

    /**
     * Delete group
     */
    deleteGroup(groupId) {
        if (!this.groups.has(groupId)) {
            Toast.warning('Groupe introuvable');
            return false;
        }

        const group = this.groups.get(groupId);
        if (confirm(`Supprimer le groupe "${group.name}" ?`)) {
            this.groups.delete(groupId);
            this.saveToStorage();
            this.updateUI();
            Toast.success('Groupe supprimé');
            
            if (this.currentGroup === groupId) {
                this.currentGroup = 'all';
                this.filterByGroup('all');
            }
            return true;
        }
        return false;
    }

    /**
     * Add camera to group
     */
    addToGroup(cameraId, groupId) {
        if (!this.groups.has(groupId)) {
            Toast.error('Groupe introuvable');
            return false;
        }

        const group = this.groups.get(groupId);
        if (!group.cameras.includes(cameraId)) {
            group.cameras.push(cameraId);
            this.saveToStorage();
            this.updateUI();
            Toast.success(`Ajouté au groupe "${group.name}"`);
        } else {
            Toast.info('Caméra déjà dans ce groupe');
        }
        return true;
    }

    /**
     * Remove camera from group
     */
    removeFromGroup(cameraId, groupId) {
        if (!this.groups.has(groupId)) return false;

        const group = this.groups.get(groupId);
        const index = group.cameras.indexOf(cameraId);
        
        if (index > -1) {
            group.cameras.splice(index, 1);
            this.saveToStorage();
            this.updateUI();
            Toast.info(`Retiré du groupe "${group.name}"`);
            return true;
        }
        return false;
    }

    /**
     * Get cameras in group
     */
    getGroupCameras(groupId) {
        if (!this.groups.has(groupId)) return [];
        return this.groups.get(groupId).cameras;
    }

    /**
     * Get all groups
     */
    getAllGroups() {
        return Array.from(this.groups.entries()).map(([id, group]) => ({
            id,
            ...group
        }));
    }

    /**
     * Filter cameras by group
     */
    filterByGroup(groupId) {
        this.currentGroup = groupId;
        
        // Update UI
        document.querySelectorAll('[data-group-filter]').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.groupFilter === groupId);
        });

        // Dispatch event for camera manager
        window.dispatchEvent(new CustomEvent('group-filter-changed', {
            detail: { groupId }
        }));
    }

    /**
     * Get current filter
     */
    getCurrentFilter() {
        return this.currentGroup;
    }

    /**
     * Update UI elements
     */
    updateUI() {
        this.updateSidebar();
        this.updateGroupStats();
    }

    /**
     * Update sidebar with groups
     */
    updateSidebar() {
        const sidebar = document.getElementById('groupsSidebar');
        if (!sidebar) return;

        const favoriteCount = this.favorites.size;
        const groupsHTML = this.getAllGroups().map(group => {
            const count = group.cameras.length;
            const isActive = this.currentGroup === group.id;
            
            return `
                <button 
                    class="group-item ${isActive ? 'active' : ''}" 
                    data-group-filter="${group.id}"
                    onclick="cameraGroups.filterByGroup('${group.id}')"
                >
                    <span class="group-icon">${group.icon}</span>
                    <span class="group-name">${group.name}</span>
                    <span class="group-count">${count}</span>
                    <button 
                        class="group-delete" 
                        onclick="event.stopPropagation(); cameraGroups.deleteGroup('${group.id}')"
                        title="Supprimer"
                    >×</button>
                </button>
            `;
        }).join('');

        sidebar.innerHTML = `
            <div class="sidebar-section">
                <div class="sidebar-section-title">📋 Filtres</div>
                <button 
                    class="group-item ${this.currentGroup === 'all' ? 'active' : ''}" 
                    data-group-filter="all"
                    onclick="cameraGroups.filterByGroup('all')"
                >
                    <span class="group-icon">📹</span>
                    <span class="group-name">Toutes</span>
                    <span class="group-count" id="allCamerasCount">0</span>
                </button>
                <button 
                    class="group-item ${this.currentGroup === 'favorites' ? 'active' : ''}" 
                    data-group-filter="favorites"
                    onclick="cameraGroups.filterByGroup('favorites')"
                >
                    <span class="group-icon">⭐</span>
                    <span class="group-name">Favoris</span>
                    <span class="group-count">${favoriteCount}</span>
                </button>
            </div>
            
            <div class="sidebar-section">
                <div class="sidebar-section-title">
                    📁 Groupes
                    <button class="btn-add-group" onclick="cameraGroups.showCreateGroupModal()" title="Nouveau groupe">+</button>
                </div>
                ${groupsHTML}
            </div>
        `;
    }

    /**
     * Update group statistics
     */
    updateGroupStats() {
        // Update counts in UI
        const allCount = document.getElementById('allCamerasCount');
        if (allCount && window.cameraManager) {
            allCount.textContent = window.cameraManager.cameras.length;
        }
    }

    /**
     * Show create group modal
     */
    showCreateGroupModal() {
        const name = prompt('Nom du groupe:');
        if (!name) return;

        const icons = ['📹', '🚪', '🚗', '💼', '🌳', '🏠', '🏢', '🏪', '🎯', '🔒'];
        const icon = prompt('Choisir un icône:\n' + icons.join(' ') + '\n(Par défaut: 📹)') || '📹';

        const groupId = name.toLowerCase().replace(/\s+/g, '-');
        this.createGroup(groupId, name, icon);
    }

    /**
     * Show manage camera groups modal
     */
    showManageCameraModal(cameraId, cameraName) {
        const groups = this.getAllGroups();
        const cameraGroups = groups.filter(g => g.cameras.includes(cameraId));
        
        let html = `
            <div style="padding: 20px;">
                <h3 style="margin: 0 0 16px 0;">📁 Gérer les groupes - ${cameraName}</h3>
                <div style="display: flex; flex-direction: column; gap: 8px;">
        `;

        groups.forEach(group => {
            const isInGroup = group.cameras.includes(cameraId);
            html += `
                <label style="display: flex; align-items: center; gap: 8px; padding: 8px; background: ${isInGroup ? '#e7f3ff' : '#f5f5f5'}; border-radius: 8px; cursor: pointer;">
                    <input 
                        type="checkbox" 
                        ${isInGroup ? 'checked' : ''}
                        onchange="cameraGroups.toggleCameraInGroup('${cameraId}', '${group.id}', this.checked)"
                    >
                    <span style="font-size: 20px;">${group.icon}</span>
                    <span style="flex: 1;">${group.name}</span>
                    <span style="color: #666; font-size: 12px;">${group.cameras.length} caméras</span>
                </label>
            `;
        });

        html += `
                </div>
                <button 
                    onclick="document.getElementById('manageCameraModal').remove()" 
                    style="margin-top: 16px; padding: 8px 16px; background: #1877f2; color: white; border: none; border-radius: 8px; cursor: pointer; width: 100%;"
                >
                    Fermer
                </button>
            </div>
        `;

        const modal = document.createElement('div');
        modal.id = 'manageCameraModal';
        modal.style.cssText = 'position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.8); display: flex; align-items: center; justify-content: center; z-index: 10000;';
        modal.innerHTML = `<div style="background: white; border-radius: 12px; max-width: 500px; max-height: 80vh; overflow-y: auto;">${html}</div>`;
        
        document.body.appendChild(modal);
        
        modal.onclick = (e) => {
            if (e.target === modal) modal.remove();
        };
    }

    /**
     * Toggle camera in group
     */
    toggleCameraInGroup(cameraId, groupId, shouldAdd) {
        if (shouldAdd) {
            this.addToGroup(cameraId, groupId);
        } else {
            this.removeFromGroup(cameraId, groupId);
        }
    }

    /**
     * Export groups configuration
     */
    exportConfig() {
        const config = {
            favorites: [...this.favorites],
            groups: Array.from(this.groups.entries()).map(([id, group]) => ({
                id,
                ...group
            })),
            exportDate: new Date().toISOString(),
            version: '1.0'
        };

        const blob = new Blob([JSON.stringify(config, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `camera-groups-${Date.now()}.json`;
        a.click();
        URL.revokeObjectURL(url);

        Toast.success('Configuration exportée');
    }

    /**
     * Import groups configuration
     */
    importConfig(file) {
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const config = JSON.parse(e.target.result);
                
                if (config.favorites) {
                    this.favorites = new Set(config.favorites);
                }
                
                if (config.groups) {
                    config.groups.forEach(group => {
                        this.groups.set(group.id, {
                            name: group.name,
                            icon: group.icon,
                            cameras: group.cameras
                        });
                    });
                }

                this.saveToStorage();
                this.updateUI();
                Toast.success('Configuration importée');
            } catch (error) {
                console.error('Import failed:', error);
                Toast.error('Échec de l\'importation');
            }
        };
        reader.readAsText(file);
    }
}

// Global instance
window.cameraGroups = new CameraGroupsManager();

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    if (window.cameraGroups) {
        cameraGroups.updateUI();
    }
});

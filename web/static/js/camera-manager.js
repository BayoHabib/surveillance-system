/**
 * Camera Manager Module
 * Handles camera grid display, filtering, and interactions
 */

class CameraManager {
    constructor() {
        this.cameras = [];
        this.filteredCameras = [];
        this.currentLayout = '2x2';
        this.currentFilter = 'all';
        this.searchQuery = '';
        this.websocket = null;
        
        this.init();
    }

    async init() {
        // Load cameras from API
        await this.loadCameras();
        
        // Setup WebSocket connection
        this.setupWebSocket();
        
        // Setup event listeners
        this.setupEventListeners();
        
        // Initial render
        this.render();
        
        // Update stats
        this.updateStats();
    }

    async loadCameras() {
        try {
            const response = await API.getCameras();
            this.cameras = response.cameras || [];
            this.filteredCameras = [...this.cameras];
        } catch (error) {
            console.error('Failed to load cameras:', error);
            Toast.error('Impossible de charger les caméras');
        }
    }

    setupWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsURL = `${protocol}//${window.location.host}/ws`;
        
        this.websocket = new WebSocketManager(wsURL);
        this.websocket.connect();
        
        this.websocket.on('message', (data) => this.handleWebSocketMessage(data));
        this.websocket.on('open', () => {
            Toast.success('Connexion établie');
        });
        this.websocket.on('error', () => {
            Toast.error('Erreur de connexion WebSocket');
        });
    }

    handleWebSocketMessage(data) {
        switch (data.type) {
            case 'camera_status':
                this.updateCameraStatus(data.camera_id, data.status);
                break;
            case 'camera_stats':
                this.updateCameraStats(data.camera_id, data.stats);
                break;
            case 'motion_detected':
                this.handleMotionAlert(data);
                break;
            case 'intrusion_detected':
                this.handleIntrusionAlert(data);
                break;
            default:
                console.log('Unknown message type:', data.type);
        }
    }

    updateCameraStatus(cameraId, status) {
        const camera = this.cameras.find(c => c.id === cameraId);
        if (camera) {
            camera.status = status;
            this.render();
            this.updateStats();
        }
    }

    updateCameraStats(cameraId, stats) {
        const camera = this.cameras.find(c => c.id === cameraId);
        if (camera) {
            camera.fps = stats.fps;
            camera.resolution = stats.resolution;
            this.render();
        }
    }

    handleMotionAlert(data) {
        Toast.warning(`Mouvement détecté sur ${data.camera_name}`);
        
        if (window.notificationManager) {
            notificationManager.sendMotionAlert(data.camera_id, data.camera_name);
        }
    }

    handleIntrusionAlert(data) {
        Toast.error(`Intrusion détectée sur ${data.camera_name}`, '', 0);
        
        if (window.notificationManager) {
            notificationManager.sendIntrusionAlert(data.camera_id, data.camera_name);
        }
    }

    setupEventListeners() {
        // Search
        const searchInput = document.getElementById('searchInput');
        if (searchInput) {
            searchInput.addEventListener('input', (e) => {
                this.searchQuery = e.target.value.toLowerCase();
                this.applyFilters();
            });
        }

        // Filter buttons
        document.querySelectorAll('[data-filter]').forEach(btn => {
            btn.addEventListener('click', (e) => {
                this.currentFilter = e.target.dataset.filter;
                this.updateFilterButtons();
                this.applyFilters();
            });
        });

        // Layout buttons
        document.querySelectorAll('[data-layout]').forEach(btn => {
            btn.addEventListener('click', (e) => {
                this.currentLayout = e.target.dataset.layout;
                this.updateLayoutButtons();
                this.updateGridLayout();
            });
        });

        // Bulk actions
        const startAllBtn = document.getElementById('startAllBtn');
        if (startAllBtn) {
            startAllBtn.addEventListener('click', () => this.startAllCameras());
        }

        const stopAllBtn = document.getElementById('stopAllBtn');
        if (stopAllBtn) {
            stopAllBtn.addEventListener('click', () => this.stopAllCameras());
        }
        
        // Listen for group filter changes
        window.addEventListener('group-filter-changed', (e) => {
            this.applyFilters();
        });
        
        // Listen for favorites changes
        window.addEventListener('favorites-changed', (e) => {
            this.render();
        });
    }

    applyFilters() {
        this.filteredCameras = this.cameras.filter(camera => {
            // Search filter
            const matchesSearch = !this.searchQuery || 
                camera.name.toLowerCase().includes(this.searchQuery) ||
                camera.id.toLowerCase().includes(this.searchQuery);

            // Status filter
            const matchesFilter = this.currentFilter === 'all' || 
                camera.status === this.currentFilter;

            // Group filter
            let matchesGroup = true;
            if (window.cameraGroups) {
                const currentGroup = cameraGroups.getCurrentFilter();
                if (currentGroup === 'favorites') {
                    matchesGroup = cameraGroups.isFavorite(camera.id);
                } else if (currentGroup !== 'all') {
                    const groupCameras = cameraGroups.getGroupCameras(currentGroup);
                    matchesGroup = groupCameras.includes(camera.id);
                }
            }

            return matchesSearch && matchesFilter && matchesGroup;
        });

        this.render();
    }

    updateFilterButtons() {
        document.querySelectorAll('[data-filter]').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.filter === this.currentFilter);
        });
    }

    updateLayoutButtons() {
        document.querySelectorAll('[data-layout]').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.layout === this.currentLayout);
        });
    }

    updateGridLayout() {
        const grid = document.getElementById('videoGrid');
        if (grid) {
            grid.className = `video-grid layout-${this.currentLayout}`;
        }
    }

    render() {
        const grid = document.getElementById('videoGrid');
        if (!grid) return;

        if (this.filteredCameras.length === 0) {
            grid.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">📹</div>
                    <h3>Aucune caméra trouvée</h3>
                    <p>Aucune caméra ne correspond à votre recherche</p>
                </div>
            `;
            return;
        }

        grid.innerHTML = this.filteredCameras.map(camera => this.renderCameraCard(camera)).join('');

        // Attach event listeners to camera cards
        this.attachCardEventListeners();
    }

    renderCameraCard(camera) {
        const statusClass = camera.status || 'offline';
        const statusText = {
            streaming: 'En ligne',
            offline: 'Hors ligne',
            error: 'Erreur'
        }[statusClass] || 'Inconnu';

        // Check if camera is favorite
        const isFavorite = window.cameraGroups ? cameraGroups.isFavorite(camera.id) : false;
        
        // Get camera groups
        let cameraGroupsList = [];
        if (window.cameraGroups) {
            const allGroups = cameraGroups.getAllGroups();
            cameraGroupsList = allGroups.filter(g => g.cameras.includes(camera.id));
        }

        return `
            <div class="camera-card" data-camera-id="${camera.id}">
                <div class="camera-video-container">
                    ${camera.status === 'streaming' 
                        ? `<video class="camera-video" autoplay muted></video>`
                        : `<div class="camera-placeholder">📹</div>`
                    }
                    
                    <!-- Favorite Star -->
                    <div class="camera-favorite ${isFavorite ? 'is-favorite' : ''}" 
                         onclick="cameraGroups.toggleFavorite('${camera.id}')" 
                         title="${isFavorite ? 'Retirer des favoris' : 'Ajouter aux favoris'}">
                        <span class="favorite-icon">⭐</span>
                    </div>
                    
                    <!-- Group Badges -->
                    ${cameraGroupsList.length > 0 ? `
                        <div class="camera-groups-badge">
                            ${cameraGroupsList.slice(0, 2).map(g => `
                                <span class="group-badge">
                                    <span class="group-badge-icon">${g.icon}</span>
                                    ${g.name}
                                </span>
                            `).join('')}
                            ${cameraGroupsList.length > 2 ? `
                                <span class="group-badge">+${cameraGroupsList.length - 2}</span>
                            ` : ''}
                        </div>
                    ` : ''}
                    
                    <div class="camera-overlay">
                        <button class="btn btn-icon" onclick="cameraManager.takeSnapshot('${camera.id}')" title="Capture">
                            📷
                        </button>
                        <button class="btn btn-icon" onclick="cameraManager.toggleFullscreen('${camera.id}')" title="Plein écran">
                            ⛶
                        </button>
                        ${camera.status === 'streaming' ? `
                            <button class="btn btn-icon pip-btn" onclick="videoZoom.togglePiP('${camera.id}')" title="Picture-in-Picture">
                                🖼️
                            </button>
                        ` : ''}
                    </div>
                </div>
                <div class="camera-info">
                    <div class="camera-header">
                        <h3 class="camera-name">${camera.name}</h3>
                        <span class="camera-status ${statusClass}">
                            <span class="status-dot"></span>
                            ${statusText}
                        </span>
                    </div>
                    <div class="camera-meta">
                        <span class="camera-meta-item">📍 ${camera.location || 'Non spécifié'}</span>
                        <span class="camera-meta-item">🎞️ ${camera.fps || 0} FPS</span>
                        <span class="camera-meta-item">📐 ${camera.resolution || 'N/A'}</span>
                    </div>
                    <div class="camera-actions">
                        ${camera.status === 'streaming'
                            ? `<button class="btn btn-danger btn-sm" onclick="cameraManager.stopCamera('${camera.id}')">⏹ Arrêter</button>`
                            : `<button class="btn btn-success btn-sm" onclick="cameraManager.startCamera('${camera.id}')">▶ Démarrer</button>`
                        }
                        <button class="btn btn-secondary btn-sm" onclick="cameraManager.showSettings('${camera.id}')">⚙️ Paramètres</button>
                        <button class="btn btn-secondary btn-sm" onclick="cameraGroups.showManageCameraModal('${camera.id}', '${camera.name}')">📁 Groupes</button>
                    </div>
                </div>
            </div>
        `;
    }

    attachCardEventListeners() {
        // Setup video streams for active cameras
        this.filteredCameras.forEach(camera => {
            if (camera.status === 'streaming') {
                const videoElement = document.querySelector(`[data-camera-id="${camera.id}"] video`);
                if (videoElement && !videoElement.src) {
                    // Setup video stream (WebRTC or other method)
                    this.setupVideoStream(camera.id, videoElement);
                    
                    // Initialize zoom controller
                    if (window.videoZoom) {
                        videoZoom.init(camera.id, videoElement);
                    }
                }
            }
        });
    }

    setupVideoStream(cameraId, videoElement) {
        // TODO: Implement actual video stream setup
        // This would typically use WebRTC, HLS, or direct stream URL
        console.log(`Setting up video stream for camera ${cameraId}`);
    }

    async startCamera(cameraId) {
        try {
            await API.startCamera(cameraId);
            Toast.success('Caméra démarrée');
        } catch (error) {
            console.error('Failed to start camera:', error);
            Toast.error('Échec du démarrage de la caméra');
        }
    }

    async stopCamera(cameraId) {
        try {
            await API.stopCamera(cameraId);
            Toast.info('Caméra arrêtée');
        } catch (error) {
            console.error('Failed to stop camera:', error);
            Toast.error('Échec de l\'arrêt de la caméra');
        }
    }

    async startAllCameras() {
        Toast.info('Démarrage de toutes les caméras...');
        
        for (const camera of this.cameras) {
            if (camera.status !== 'streaming') {
                await this.startCamera(camera.id);
                await new Promise(resolve => setTimeout(resolve, 500));
            }
        }
        
        Toast.success('Toutes les caméras ont été démarrées');
    }

    async stopAllCameras() {
        Toast.info('Arrêt de toutes les caméras...');
        
        for (const camera of this.cameras) {
            if (camera.status === 'streaming') {
                await this.stopCamera(camera.id);
                await new Promise(resolve => setTimeout(resolve, 200));
            }
        }
        
        Toast.info('Toutes les caméras ont été arrêtées');
    }

    takeSnapshot(cameraId) {
        const videoElement = document.querySelector(`[data-camera-id="${cameraId}"] video`);
        if (!videoElement) {
            Toast.warning('Aucune vidéo active pour cette caméra');
            return;
        }

        // Get camera info
        const camera = this.cameras.find(c => c.id === cameraId);
        const cameraInfo = {
            id: cameraId,
            name: camera?.name || 'Caméra Inconnue',
            location: camera?.location || ''
        };

        // Open snapshot editor
        if (window.snapshotEditor) {
            snapshotEditor.open(videoElement, cameraInfo);
        } else {
            // Fallback to simple snapshot
            const canvas = document.createElement('canvas');
            canvas.width = videoElement.videoWidth;
            canvas.height = videoElement.videoHeight;
            
            const ctx = canvas.getContext('2d');
            ctx.drawImage(videoElement, 0, 0);
            
            canvas.toBlob(blob => {
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `snapshot-${cameraId}-${Date.now()}.png`;
                a.click();
                URL.revokeObjectURL(url);
                
                Toast.success('Capture enregistrée');
            });
        }
    }

    toggleFullscreen(cameraId) {
        const card = document.querySelector(`[data-camera-id="${cameraId}"]`);
        if (!card) return;

        card.classList.toggle('fullscreen');
        
        if (card.classList.contains('fullscreen')) {
            document.body.style.overflow = 'hidden';
        } else {
            document.body.style.overflow = '';
        }
    }

    showSettings(cameraId) {
        // TODO: Implement settings modal
        Toast.info('Paramètres de la caméra');
        console.log('Show settings for camera:', cameraId);
    }

    updateStats() {
        const activeCameras = this.cameras.filter(c => c.status === 'streaming').length;
        const totalCameras = this.cameras.length;
        const avgFps = this.cameras.reduce((sum, c) => sum + (c.fps || 0), 0) / totalCameras || 0;
        
        // Update stat cards
        this.updateStatCard('activeCameras', activeCameras);
        this.updateStatCard('totalCameras', totalCameras);
        this.updateStatCard('avgFps', Math.round(avgFps));
        
        // Update group stats
        if (window.cameraGroups) {
            cameraGroups.updateGroupStats();
        }
    }

    updateStatCard(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
        }
    }
}

// Initialize camera manager when DOM is ready
let cameraManager;
document.addEventListener('DOMContentLoaded', () => {
    cameraManager = new CameraManager();
});

// Export for global access
window.CameraManager = CameraManager;
